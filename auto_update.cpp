#include "auto_update.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <mbedtls/sha256.h>
#include <time.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#include "bulb_registry.h"
#include "config.h"
#include "debug_log.h"
#include "ota_update.h"
#include "tls_bundle_client.h"
#include "update_check.h"
#include "web_hooks.h"

namespace {

#if defined(MQTT_HOST)
constexpr bool SUPPORTED = false;
#else
constexpr bool SUPPORTED = true;
#endif

enum class Mode : uint8_t { Off = 0, Notify = 1, Auto = 2 };
enum class Job : uint8_t { None = 0, Check, Install };

// Shared with the worker task: the task fills it in, then raises `done`; the
// main loop consumes it and resets `job`.
struct TaskState {
  volatile Job job = Job::None;
  volatile bool done = false;
  volatile bool ok = false;
  volatile uint32_t downloaded = 0;
  volatile uint32_t total = 0;
  char tag[24] = "";
  char error[48] = "";
};
TaskState ts;

constexpr char PREFS_NS[] = "update";

Mode mode = Mode::Off;
char latest[24] = "";
char lastError[48] = "";
char notifiedTag[24] = "";
char badTag[24] = "";
char pendingTryTag[24] = "";   // Installed, waiting for the boot confirmation
bool installIsAuto = false;
bool rebooting = false;
bool haveChecked = false;
uint32_t lastCheckMs = 0;
uint32_t nextCheckMs = UPDATE_FIRST_CHECK_MS;

bool relevant(uint32_t atMs) {
  return (int32_t)(millis() - atMs) >= 0;
}

void taskFail(const char *message) {
  strlcpy(ts.error, message, sizeof(ts.error));
  ts.ok = false;
}

void taskFailCode(const char *message, int code) {
  snprintf(ts.error, sizeof(ts.error), "%s (%d)", message, code);
  ts.ok = false;
}

String userAgent() {
  String ua = "esp32-zigbee-bulb/";
  ua += FW_VERSION;
  return ua;
}

void prepare(HTTPClient &http, followRedirects_t follow) {
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  http.setFollowRedirects(follow);
  http.setUserAgent(userAgent());
}

void runCheck() {
  BundleVerifyingClient client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  prepare(http, HTTPC_DISABLE_FOLLOW_REDIRECTS);
  static const char *keys[] = {"Location"};
  http.collectHeaders(keys, 1);

  String url = "https://github.com/";
  url += UPDATE_REPO;
  url += "/releases/latest";
  if (!http.begin(client, url)) {
    taskFail("begin failed");
    return;
  }
  const int code = http.GET();
  const String location = http.header("Location");
  http.end();
  if (code == 404) {
    taskFail("no release published");
    return;
  }
  if (code < 300 || code >= 400) {
    taskFailCode("github unreachable", code);
    return;
  }
  String tag;
  if (!tagFromLocation(location, UPDATE_REPO, tag) || tag.length() >= sizeof(ts.tag)) {
    taskFail("unexpected redirect");
    return;
  }
  strlcpy(ts.tag, tag.c_str(), sizeof(ts.tag));
  ts.ok = true;
}

bool fetchChecksum(const String &tag, uint8_t out[32]) {
  BundleVerifyingClient client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  prepare(http, HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(client, releaseAssetUrl(UPDATE_REPO, tag, UPDATE_SHA_ASSET))) {
    taskFail("begin failed");
    return false;
  }
  const int code = http.GET();
  if (code != 200) {
    http.end();
    taskFailCode("checksum download", code);
    return false;
  }
  const int size = http.getSize();
  if (size <= 0 || size > 512) {
    http.end();
    taskFail("bad checksum file");
    return false;
  }
  const String body = http.getString();
  http.end();
  if (!parseSha256Hex(body, out)) {
    taskFail("bad checksum file");
    return false;
  }
  return true;
}

struct Sha256 {
  mbedtls_sha256_context ctx;
  Sha256() {
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
  }
  ~Sha256() { mbedtls_sha256_free(&ctx); }
};

// Streams the release image into the inactive slot. On any failure the flash
// write is aborted and ts.error says why. Returns true once the image is
// written, verified against `expected` and the boot partition is switched.
bool downloadAndFlash(const String &tag, const uint8_t expected[32]) {
  BundleVerifyingClient client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  prepare(http, HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(client, releaseAssetUrl(UPDATE_REPO, tag, UPDATE_ASSET))) {
    taskFail("begin failed");
    return false;
  }
  const int code = http.GET();
  if (code != 200) {
    http.end();
    taskFailCode("firmware download", code);
    return false;
  }
  const int size = http.getSize();
  if (size <= 0 || (size_t)size > UPDATE_MAX_BYTES) {
    http.end();
    taskFail("unexpected firmware size");
    return false;
  }
  ts.total = (uint32_t)size;

  otaUploadStart(String());
  if (!otaUploading()) {
    http.end();
    taskFail("cannot start flash write");
    return false;
  }

  Sha256 sha;
  auto *stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t remaining = (size_t)size;
  uint32_t lastDataMs = millis();
  while (remaining > 0) {
    if (!otaUploading()) {  // The flash layer gave up (write error).
      http.end();
      taskFail("flash write failed");
      return false;
    }
    const int avail = stream->available();
    if (avail <= 0) {
      if (!stream->connected()) {
        otaUploadAbort();
        http.end();
        taskFail("connection lost");
        return false;
      }
      if (millis() - lastDataMs > UPDATE_STALL_MS) {
        otaUploadAbort();
        http.end();
        taskFail("download stalled");
        return false;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    size_t want = (size_t)avail;
    if (want > remaining) want = remaining;
    if (want > sizeof(buf)) want = sizeof(buf);
    const size_t n = stream->readBytes(buf, want);
    if (n == 0) continue;
    mbedtls_sha256_update(&sha.ctx, buf, n);
    otaUploadWrite(buf, n);
    remaining -= n;
    ts.downloaded = ts.downloaded + (uint32_t)n;
    lastDataMs = millis();
  }
  http.end();

  uint8_t digest[32];
  mbedtls_sha256_finish(&sha.ctx, digest);
  if (!sha256Equal(digest, expected)) {
    otaUploadAbort();
    taskFail("checksum mismatch");
    return false;
  }

  // Remember the tag before the reboot is scheduled so a rollback is noticed.
  Preferences prefs;
  prefs.begin(PREFS_NS, false);
  prefs.putString("try", tag);
  prefs.end();
  otaUploadEnd();
  if (otaResult() != OtaResult::Done) {
    Preferences undo;
    undo.begin(PREFS_NS, false);
    undo.remove("try");
    undo.end();
    taskFail("image rejected");
    return false;
  }
  return true;
}

void runInstall() {
  const String tag(ts.tag);
  uint8_t expected[32];
  if (!fetchChecksum(tag, expected)) return;
  ts.ok = downloadAndFlash(tag, expected);
}

void workerTask(void *) {
  if (ts.job == Job::Check) {
    runCheck();
  } else {
    runInstall();
  }
  ts.done = true;
  vTaskDelete(nullptr);
}

bool startJob(Job job, const char *tag) {
  if (ts.job != Job::None) return false;
  ts.done = false;
  ts.ok = false;
  ts.error[0] = '\0';
  ts.downloaded = 0;
  ts.total = 0;
  strlcpy(ts.tag, tag, sizeof(ts.tag));
  ts.job = job;
  if (xTaskCreate(workerTask, "update", 10240, nullptr, 1, nullptr) != pdPASS) {
    ts.job = Job::None;
    strlcpy(lastError, "task create failed", sizeof(lastError));
    return false;
  }
  return true;
}

bool newerAvailable() {
  return latest[0] != '\0' && versionNewer(String(FW_VERSION), String(latest));
}

bool timeReady() {
  if (time(nullptr) > 1700000000) return true;
  if (!esp_sntp_enabled()) {
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  }
  return false;
}

void rememberBad(const char *tag) {
  strlcpy(badTag, tag, sizeof(badTag));
  Preferences prefs;
  prefs.begin(PREFS_NS, false);
  prefs.putString("bad", tag);
  prefs.end();
}

void consumeResult() {
  const Job job = ts.job;
  if (job == Job::Check) {
    lastCheckMs = millis();
    haveChecked = true;
    if (ts.ok) {
      strlcpy(latest, ts.tag, sizeof(latest));
      lastError[0] = '\0';
      nextCheckMs = millis() + UPDATE_CHECK_INTERVAL_MS;
      debugLogPrintf("Update: latest release %s (running %s)\n", latest, FW_VERSION);
      if (newerAvailable() && strcmp(notifiedTag, latest) != 0) {
        strlcpy(notifiedTag, latest, sizeof(notifiedTag));
        webHookEvent("update_available", "", "", latest);
      }
    } else {
      strlcpy(lastError, ts.error, sizeof(lastError));
      nextCheckMs = millis() + UPDATE_RETRY_MS;
      debugLogPrintf("Update: check failed: %s\n", lastError);
    }
  } else if (job == Job::Install) {
    if (ts.ok) {
      rebooting = true;
      lastError[0] = '\0';
      debugLogPrintf("Update: %s installed, rebooting\n", ts.tag);
    } else {
      strlcpy(lastError, ts.error, sizeof(lastError));
      debugLogPrintf("Update: install failed: %s\n", lastError);
      if (installIsAuto) rememberBad(ts.tag);  // Do not retry by itself.
    }
  }
  ts.job = Job::None;
  ts.done = false;
}

}  // namespace

bool updateSupported() { return SUPPORTED; }

void updateBegin() {
  Preferences prefs;
  prefs.begin(PREFS_NS, false);
  const uint8_t stored = prefs.getUChar("mode", (uint8_t)Mode::Off);
  mode = stored <= (uint8_t)Mode::Auto ? (Mode)stored : Mode::Off;
  strlcpy(badTag, prefs.getString("bad", "").c_str(), sizeof(badTag));
  const String tried = prefs.getString("try", "");
  if (tried.length() > 0) {
    if (sameVersion(String(FW_VERSION), tried)) {
      // Running the new image: keep "try" until the boot is confirmed.
      strlcpy(pendingTryTag, tried.c_str(), sizeof(pendingTryTag));
    } else {
      // We asked for this version but are still on the old one: rolled back.
      prefs.remove("try");
      prefs.putString("bad", tried);
      strlcpy(badTag, tried.c_str(), sizeof(badTag));
      debugLogPrintf("Update: %s was rolled back; automatic installs skip it\n", tried.c_str());
    }
  }
  prefs.end();
  if (!SUPPORTED) mode = Mode::Off;
}

void updateTick() {
  if (ts.job != Job::None && ts.done) consumeResult();

  if (pendingTryTag[0] != '\0' && !otaPendingVerify()) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.remove("try");
    prefs.remove("bad");
    prefs.end();
    pendingTryTag[0] = '\0';
    badTag[0] = '\0';
    debugLogPrintln("Update: new firmware confirmed");
  }

  if (!SUPPORTED || rebooting || otaUploading() || ts.job != Job::None) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (mode != Mode::Off && relevant(nextCheckMs)) {
    if (!timeReady()) {
      nextCheckMs = millis() + 10000;
      return;
    }
    nextCheckMs = millis() + UPDATE_RETRY_MS;  // Replaced when the check ends.
    startJob(Job::Check, "");
    return;
  }

  if (mode == Mode::Auto && newerAvailable() && strcmp(latest, badTag) != 0 &&
      millis() > UPDATE_AUTO_MIN_UPTIME_MS && !otaPendingVerify()) {
    installIsAuto = true;
    registryFlush();  // No NVS write while the flash is being written.
    startJob(Job::Install, latest);
  }
}

bool updateSetMode(const String &name) {
  if (!SUPPORTED) return false;
  Mode m;
  if (name == "off") m = Mode::Off;
  else if (name == "notify") m = Mode::Notify;
  else if (name == "auto") m = Mode::Auto;
  else return false;
  mode = m;
  Preferences prefs;
  prefs.begin(PREFS_NS, false);
  prefs.putUChar("mode", (uint8_t)m);
  prefs.end();
  if (m != Mode::Off && !haveChecked) nextCheckMs = millis();  // Check soon.
  return true;
}

bool updateCheckNow() {
  if (!SUPPORTED || ts.job != Job::None || rebooting) return false;
  if (WiFi.status() != WL_CONNECTED || !timeReady()) return false;
  return startJob(Job::Check, "");
}

bool updateInstallNow() {
  if (!SUPPORTED || ts.job != Job::None || rebooting || otaUploading()) return false;
  if (WiFi.status() != WL_CONNECTED || !newerAvailable()) return false;
  installIsAuto = false;
  if (strcmp(latest, badTag) == 0) {  // The user overrides the skip.
    badTag[0] = '\0';
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.remove("bad");
    prefs.end();
  }
  registryFlush();
  return startJob(Job::Install, latest);
}

String updateAvailableVersion() {
  return newerAvailable() ? String(latest) : String();
}

String updateStatusJson() {
  const char *modeName = mode == Mode::Auto ? "auto" : mode == Mode::Notify ? "notify" : "off";
  const char *state = "idle";
  if (rebooting) state = "rebooting";
  else if (ts.job == Job::Check) state = "checking";
  else if (ts.job == Job::Install) state = "downloading";
  const uint32_t total = ts.total;
  const uint32_t pct = total > 0 ? (uint32_t)((uint64_t)ts.downloaded * 100 / total) : 0;
  String j;
  j.reserve(260);
  j += "{\"supported\":";
  j += SUPPORTED ? "true" : "false";
  j += ",\"mode\":\"";
  j += modeName;
  j += "\",\"current\":\"";
  j += FW_VERSION;
  j += "\",\"latest\":\"";
  j += latest;
  j += "\",\"available\":";
  j += newerAvailable() ? "true" : "false";
  j += ",\"state\":\"";
  j += state;
  j += "\",\"progress\":";
  j += pct;
  j += ",\"error\":\"";
  j += lastError;
  j += "\",\"last_check_s\":";
  j += haveChecked ? (long)((millis() - lastCheckMs) / 1000) : -1L;
  j += ",\"auto_blocked\":";
  j += (newerAvailable() && strcmp(latest, badTag) == 0) ? "true" : "false";
  j += "}";
  return j;
}

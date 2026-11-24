#include "bulb_registry.h"

#include <Preferences.h>

#include "config.h"
#include "debug_log.h"
#include "name_sanitize.h"
#include "status_led.h"

namespace {

Bulb bulbs[MAX_BULBS];
size_t bulbCountValue = 0;
bool stateDirty = false;
uint32_t stateChangedAtMs = 0;
Preferences prefs;

uint8_t pctToLevel(uint8_t pct) {
  return (uint8_t)((pct * 255 + 50) / 100);
}

uint8_t levelToPct(uint8_t level) {
  return (uint8_t)((level * 100 + 127) / 255);
}

bool ieeeEquals(const esp_zb_ieee_addr_t a, const esp_zb_ieee_addr_t b) {
  return memcmp(a, b, sizeof(esp_zb_ieee_addr_t)) == 0;
}

const esp_zb_ieee_addr_t kNullIeee = {0, 0, 0, 0, 0, 0, 0, 0};

void sanitizeName(char *out, size_t cap, const char *in) {
  sanitizeDisplayName(out, cap, in, "Bulb");
}

void serializeRuntimeAddr(const Bulb &b, String &out) {
  out = String(b.shortAddr, 16);
  out += ",";
  out += String(b.endpoint);
}

// Helper to parse address strings generated in the registry format.
void deserializeRuntimeAddr(Bulb &b, const String &s) {
  int comma = s.indexOf(',');
  if (comma < 0) return;
  long shortAddr = strtol(s.substring(0, comma).c_str(), nullptr, 16);
  int endpoint = s.substring(comma + 1).toInt();
  if (shortAddr > 0 && shortAddr < 0xFFF8) b.shortAddr = (uint16_t)shortAddr;
  if (endpoint > 0 && endpoint <= 240) b.endpoint = (uint8_t)endpoint;
}

}  // namespace

// Public: shared with the scenes module (and NVS storage).
String serializeBulbState(const BulbState &st) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%u,%u,%u,%u,%u,%u,%u",
           (uint8_t)st.mode,
           st.power ? 1 : 0,
           st.level,
           st.kelvin,
           st.red,
           st.green,
           st.blue);
  return String(buf);
}

bool deserializeBulbState(BulbState &st, const String &s) {
  int v[7] = {0};
  int n = 0;
  int start = 0;
  const char *c_str = s.c_str();
  for (int i = 0; i <= (int)s.length() && n < 7; ++i) {
    if (i == (int)s.length() || c_str[i] == ',') {
      v[n++] = strtol(c_str + start, nullptr, 10);
      start = i + 1;
    }
  }
  if (n != 7) return false;
  st.mode = v[0] == 1 ? BulbColorMode::Rgb : BulbColorMode::White;
  st.power = v[1] != 0;
  st.level = (uint8_t)constrain(v[2], 0, 255);
  st.kelvin = (uint16_t)constrain(v[3], MIN_KELVIN, MAX_KELVIN);
  st.red = (uint8_t)constrain(v[4], 0, 255);
  st.green = (uint8_t)constrain(v[5], 0, 255);
  st.blue = (uint8_t)constrain(v[6], 0, 255);
  return true;
}

void registryBegin() {
  prefs.begin(PREFS_NAMESPACE, false);
  bulbCountValue = prefs.getUChar("count", 0);
  if (bulbCountValue > MAX_BULBS) bulbCountValue = MAX_BULBS;

  for (size_t i = 0; i < bulbCountValue; ++i) {
    Bulb &b = bulbs[i];
    char key[16];
    int len = snprintf(key, sizeof(key), "b%zu", i);

    if (len > 0 && len < (int)sizeof(key)) {
      String data = prefs.getString(key, "");
      if (data.length() > 0 && data.indexOf('|') >= 0) {
        int p1 = data.indexOf('|');
        int p2 = data.indexOf('|', p1 + 1);
        int p3 = data.indexOf('|', p2 + 1);

        if (p1 > 0 && p2 > p1 && p3 > p2) {
          String ieeeHex = data.substring(0, p1);
          if (ieeeHex.length() == 16) {
            for (int k = 0; k < 8; ++k) {
              const char hi = ieeeHex[14 - 2 * k];
              const char lo = ieeeHex[15 - 2 * k];
              const uint8_t hiV = hi <= '9' ? (uint8_t)(hi - '0') : (uint8_t)(tolower(hi) - 'a' + 10);
              const uint8_t loV = lo <= '9' ? (uint8_t)(lo - '0') : (uint8_t)(tolower(lo) - 'a' + 10);
              b.ieee[k] = (uint8_t)((hiV << 4) | loV);
            }
          }
          String name = data.substring(p1 + 1, p2);
          sanitizeName(b.name, sizeof(b.name), name.c_str());

          String stateStr = data.substring(p2 + 1, p3);
          deserializeBulbState(b.state, stateStr);

          String addrStr = data.substring(p3 + 1);
          deserializeRuntimeAddr(b, addrStr);
        }
      } else if (len < (int)sizeof(key) - 1) {
        // Fallback to legacy chunked schema
        key[len] = 'i';
        key[len + 1] = '\0';
        prefs.getBytes(key, b.ieee, sizeof(esp_zb_ieee_addr_t));

        key[len] = 'n';
        key[len + 1] = '\0';
        String name = prefs.getString(key, "");
        sanitizeName(b.name, sizeof(b.name), name.c_str());

        key[len] = 's';
        key[len + 1] = '\0';
        deserializeBulbState(b.state, prefs.getString(key, ""));

        key[len] = 'a';
        key[len + 1] = '\0';
        deserializeRuntimeAddr(b, prefs.getString(key, ""));
      }
    }
    b.online = false;
  }
}

size_t registryCount() {
  return bulbCountValue;
}

Bulb *registryGet(size_t index) {
  if (index >= bulbCountValue) return nullptr;
  return &bulbs[index];
}

Bulb *registryFindByIeee(const esp_zb_ieee_addr_t ieee) {
  for (size_t i = 0; i < bulbCountValue; ++i) {
    if (ieeeEquals(bulbs[i].ieee, ieee)) return &bulbs[i];
  }
  return nullptr;
}

static uint8_t hexCharToInt(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

Bulb *registryFindByIeeeHex(const char *ieeeHex, size_t length) {
  if (length != 16) return nullptr;

  esp_zb_ieee_addr_t target;
  for (int i = 0; i < 8; ++i) {
    target[7 - i] = (hexCharToInt(ieeeHex[i * 2]) << 4) | hexCharToInt(ieeeHex[i * 2 + 1]);
  }

  for (size_t i = 0; i < bulbCountValue; ++i) {
    if (ieeeEquals(bulbs[i].ieee, target)) return &bulbs[i];
  }
  return nullptr;
}

Bulb *registryAdd(const esp_zb_ieee_addr_t ieee) {
  if (ieeeEquals(ieee, kNullIeee)) return nullptr;
  if (registryFindByIeee(ieee) != nullptr) return registryFindByIeee(ieee);
  if (bulbCountValue >= MAX_BULBS) return nullptr;

  Bulb &b = bulbs[bulbCountValue];
  ++bulbCountValue;
  memcpy(b.ieee, ieee, sizeof(esp_zb_ieee_addr_t));
  b.state = BulbState();  // Sensible defaults from the struct
  b.state.level = pctToLevel(DEFAULT_BRIGHTNESS_PCT);
  b.state.kelvin = DEFAULT_KELVIN;

  // Auto name "Bulb N", skipping names already in use.
  for (int candidate = 1; candidate <= 9999; ++candidate) {
    char wanted[sizeof(b.name)];
    int n = snprintf(wanted, sizeof(wanted), "Bulb %d", candidate);
    if (n < 0 || n >= (int)sizeof(wanted)) {
      continue; // truncation or error, skip
    }
    bool taken = false;
    for (size_t i = 0; i < bulbCountValue - 1; ++i) {
      if (strcmp(bulbs[i].name, wanted) == 0) {
        taken = true;
        break;
      }
    }
    if (!taken) {
      snprintf(b.name, sizeof(b.name), "%s", wanted);
      break;
    }
  }

  registryFlush();
  debugLogPrintf("Registry: added %s (%s)\n", b.name, bulbIeeeHex(&b).c_str());
  return &b;
}

void registryRemove(Bulb *bulb) {
  if (bulb == nullptr) return;
  size_t at = bulb - bulbs;
  if (at >= bulbCountValue) return;
  for (size_t i = at; i + 1 < bulbCountValue; ++i) {
    bulbs[i] = bulbs[i + 1];
  }
  --bulbCountValue;
  stateDirty = true;
  registryFlush();
  debugLogPrintf("Registry: removed entry %u\n", (unsigned)at);
}

void registryRename(Bulb *bulb, const char *newName) {
  if (bulb == nullptr || newName == nullptr) return;
  sanitizeName(bulb->name, sizeof(bulb->name), newName);
  stateDirty = true;
  registryFlush();
}

void registryMarkDirty() {
  stateDirty = true;
  stateChangedAtMs = millis();
}

void registryTick() {
  if (stateDirty && millis() - stateChangedAtMs >= STATE_SAVE_DELAY_MS) {
    registryFlush();
  }
}

void registryFlush() {
  if (!stateDirty) return;
  stateDirty = false;
  prefs.putUChar("count", (uint8_t)bulbCountValue);
  for (size_t i = 0; i < bulbCountValue; ++i) {
    const Bulb &b = bulbs[i];
    char key[16];
    int len = snprintf(key, sizeof(key), "b%zu", i);

    if (len > 0 && len < (int)sizeof(key)) {
      char ieeeBuf[17];
      int ieeeOff = 0;
      for (int j = 7; j >= 0; --j) {
        ieeeOff += snprintf(ieeeBuf + ieeeOff, sizeof(ieeeBuf) - ieeeOff, "%02x", b.ieee[j]);
      }

      String addrStr;
      serializeRuntimeAddr(b, addrStr);

      char buf[256];
      snprintf(buf, sizeof(buf), "%s|%s|%s|%s",
               ieeeBuf,
               b.name,
               serializeBulbState(b.state).c_str(),
               addrStr.c_str());

      prefs.putString(key, String(buf));
    }
  }
}

void bulbSetBrightnessPct(Bulb *bulb, uint8_t pct) {
  if (bulb == nullptr) return;
  bulb->state.level = pctToLevel(constrain(pct, 0, 100));
  registryMarkDirty();
}

uint8_t bulbBrightnessPct(const Bulb *bulb) {
  if (bulb == nullptr) return 0;
  return levelToPct(bulb->state.level);
}

String bulbIeeeHex(const Bulb *bulb) {
  if (bulb == nullptr) return String();
  char buf[17];
  // esp_zb_ieee_addr_t is little-endian; print in network order (reverse).
  snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x%02x%02x",
           bulb->ieee[7], bulb->ieee[6], bulb->ieee[5], bulb->ieee[4],
           bulb->ieee[3], bulb->ieee[2], bulb->ieee[1], bulb->ieee[0]);
  return String(buf);
}

#include "bulb_registry.h"

#include <Preferences.h>

#include "config.h"
#include "debug_log.h"
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
  size_t o = 0;
  for (const char *p = in; *p != '\0' && o + 1 < cap; ++p) {
    char c = *p;
    if (c == '"' || c == '\\' || c < 0x20 || c > 0x7e) c = ' ';
    out[o++] = c;
  }
  while (o > 0 && out[o - 1] == ' ') o--;
  out[o] = '\0';
  if (out[0] == '\0') snprintf(out, cap, "Bulb");
}

void serializeRuntimeAddr(const Bulb &b, String &out) {
  out = String(b.shortAddr, 16);
  out += ",";
  out += String(b.endpoint);
}

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
  String s;
  s.reserve(48);
  s += String((uint8_t)st.mode);
  s += st.power ? ",1" : ",0";
  s += "," + String(st.level);
  s += "," + String(st.kelvin);
  s += "," + String(st.red);
  s += "," + String(st.green);
  s += "," + String(st.blue);
  return s;
}

bool deserializeBulbState(BulbState &st, const String &s) {
  int v[7] = {0};
  int n = 0;
  int start = 0;
  for (int i = 0; i <= (int)s.length() && n < 7; ++i) {
    if (i == (int)s.length() || s[i] == ',') {
      v[n++] = s.substring(start, i).toInt();
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
    String key = "b" + String(i);
    prefs.getBytes((key + "i").c_str(), b.ieee, sizeof(esp_zb_ieee_addr_t));
    String name = prefs.getString((key + "n").c_str(), "");
    sanitizeName(b.name, sizeof(b.name), name.c_str());
    deserializeBulbState(b.state, prefs.getString((key + "s").c_str(), ""));
    // Short address and endpoint survive reboots in practice (the network
    // resumes and bulbs keep their assignment); refreshed from the binding
    // table and reports anyway.
    deserializeRuntimeAddr(b, prefs.getString((key + "a").c_str(), ""));
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

Bulb *registryFindByIeeeHex(const String &ieeeHex) {
  for (size_t i = 0; i < bulbCountValue; ++i) {
    if (bulbIeeeHex(&bulbs[i]).equalsIgnoreCase(ieeeHex)) return &bulbs[i];
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
  for (int candidate = 1;; ++candidate) {
    char wanted[sizeof(b.name)];
    snprintf(wanted, sizeof(wanted), "Bulb %d", candidate);
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
    String key = "b" + String(i);
    prefs.putBytes((key + "i").c_str(), b.ieee, sizeof(esp_zb_ieee_addr_t));
    prefs.putString((key + "n").c_str(), b.name);
    prefs.putString((key + "s").c_str(), serializeBulbState(b.state));
    String addr;
    serializeRuntimeAddr(b, addr);
    prefs.putString((key + "a").c_str(), addr);
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
  String s;
  s.reserve(16);
  // esp_zb_ieee_addr_t is little-endian; print in network order (reverse).
  for (int i = 7; i >= 0; --i) {
    s += String(bulb->ieee[i] >> 4, HEX);
    s += String(bulb->ieee[i] & 0x0f, HEX);
  }
  s.toLowerCase();
  return s;
}

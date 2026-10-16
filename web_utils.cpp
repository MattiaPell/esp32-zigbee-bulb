#include "web_utils.h"
#include "bulb_registry.h"
#include "config.h"
#include "json_lite.h"
#include "zigbee_bulbs.h"

namespace {

bool parsePatchName(Bulb *b, const String &body, PatchContext &ctx) {
  String name;
  if (jsonGetString(body, "name", name)) {
    if (name.length() > 0 && name.length() <= 20) {
      registryRename(b, name.c_str());
      ctx.changed = true;
    } else {
      ctx.httpCode = 400;
      ctx.errorMsg = "invalid name";
      return false;
    }
  }
  return true;
}

void parsePatchPower(Bulb *b, const String &body, PatchContext &ctx) {
  bool on;
  if (jsonGetBool(body, "on", on)) {
    on ? bulbSendOn(b) : bulbSendOff(b);
    ctx.changed = true;
  }
}

void parsePatchBrightness(Bulb *b, const String &body, uint16_t tds, PatchContext &ctx) {
  long brightness;
  if (jsonGetInt(body, "brightness", brightness)) {
    bulbSendBrightness(b, (uint8_t)constrain(brightness, 0, 100), tds);
    ctx.changed = true;
  }
}

void parsePatchKelvin(Bulb *b, const String &body, uint16_t tds, PatchContext &ctx) {
  long kelvin;
  if (jsonGetInt(body, "kelvin", kelvin)) {
    bulbSendKelvin(b, (int)constrain(kelvin, MIN_KELVIN, MAX_KELVIN), tds);
    ctx.changed = true;
  }
}

bool parsePatchRgbHex(Bulb *b, const String &body, uint16_t tds, PatchContext &ctx) {
  String rgbHex;
  if (jsonGetString(body, "rgb_hex", rgbHex)) {
    uint8_t r = 0, g = 0, bl = 0;
    if (parseRgbHex(rgbHex, r, g, bl)) {
      bulbSendRgb(b, r, g, bl, tds);
      ctx.changed = true;
    } else {
      ctx.httpCode = 400;
      ctx.errorMsg = "invalid rgb_hex";
      return false;
    }
  }
  return true;
}

bool parsePatchMode(Bulb *b, const String &body, uint16_t tds, PatchContext &ctx) {
  String mode;
  if (jsonGetString(body, "mode", mode)) {
    if (mode == "white") {
      bulbSendKelvin(b, b->state.kelvin, tds);
      ctx.changed = true;
    } else if (mode == "rgb") {
      bulbSendRgb(b, b->state.red, b->state.green, b->state.blue, tds);
      ctx.changed = true;
    } else {
      ctx.httpCode = 400;
      ctx.errorMsg = "invalid mode";
      return false;
    }
  }
  return true;
}

} // namespace

bool applyLightPatch(Bulb *b, const String &body, PatchContext &ctx) {
  // Transition override: 0 = cambio colore istantaneo (usato dal flash polizia).
  long transition = (long)DEFAULT_TRANSITION_DS;
  jsonGetInt(body, "transition", transition);
  const uint16_t tds = (uint16_t)constrain(transition, 0, 6000);

  // Rename works offline; anything that drives the bulb does not.
  static const char *controlKeys[] = {"on", "brightness", "kelvin", "rgb_hex", "mode"};
  for (const char *key : controlKeys) {
    if (findJsonKey(body, key) != (size_t)-1 && !bulbReady(b)) {
      ctx.httpCode = 409;
      ctx.errorMsg = "light is offline";
      return false;
    }
  }

  if (!parsePatchName(b, body, ctx)) return false;
  parsePatchPower(b, body, ctx);
  parsePatchBrightness(b, body, tds, ctx);
  parsePatchKelvin(b, body, tds, ctx);
  if (!parsePatchRgbHex(b, body, tds, ctx)) return false;
  if (!parsePatchMode(b, body, tds, ctx)) return false;

  if (!ctx.changed) {
    ctx.httpCode = 400;
    ctx.errorMsg = "no recognized fields";
    return false;
  }

  return true;
}

bool isValidHostname(const String &name) {
  if (name.length() == 0 || name.length() > MAX_HOSTNAME_LENGTH) return false;
  if (name[0] == '-' || name[name.length() - 1] == '-') return false;
  for (unsigned i = 0; i < name.length(); ++i) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

bool parseRgbHex(const String &rgbHex, uint8_t &r, uint8_t &g, uint8_t &b) {
  String hex = rgbHex;
  if (hex.length() > 0 && hex[0] == '#') {
    hex = hex.substring(1);
  }

  if (hex.length() != 6) {
    return false;
  }

  for (unsigned i = 0; i < 6; ++i) {
    char c = hex[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      return false;
    }
  }

  unsigned rr = 0, gg = 0, bb = 0;
  if (sscanf(hex.c_str(), "%02x%02x%02x", &rr, &gg, &bb) == 3) {
    r = (uint8_t)rr;
    g = (uint8_t)gg;
    b = (uint8_t)bb;
    return true;
  }
  return false;
}

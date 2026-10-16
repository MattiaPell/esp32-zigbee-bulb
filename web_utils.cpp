#include "web_utils.h"
#include "config.h"

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

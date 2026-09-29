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

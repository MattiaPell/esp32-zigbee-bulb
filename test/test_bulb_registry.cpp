#include "check.h"
#include <cstdint>
#include <string>

#define HEX 16
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

#define millis millis_registry
#define debugLogPrintf debugLogPrintf_registry
#define registryFindByIeee registryFindByIeee_registry
#define bulbIeeeHex bulbIeeeHex_registry

uint32_t mock_millis_registry = 0;
uint32_t millis() { return mock_millis_registry; }
void debugLogPrintf(const char * /*format*/, ...) {}

#include "bulb_registry.h"

class PreferencesMock {
public:
    void begin(const char*, bool) {}
    uint8_t getUChar(const char*, uint8_t def) { return def; }
    void putUChar(const char*, uint8_t) {}
    void getBytes(const char*, void*, size_t) {}
    void putBytes(const char*, const void*, size_t) {}
    String getString(const char*, const char* def) { return String(def); }
    void putString(const char*, const String&) {}
};
#define Preferences PreferencesMock

#include "../bulb_registry.cpp"

void testSanitizeName() {
  check::begin("sanitizeName");

  char out[32];

  sanitizeName(out, sizeof(out), "Hello World");
  CHECK_EQ(std::string(out), "Hello World");

  sanitizeName(out, sizeof(out), "A\"B\\C");
  CHECK_EQ(std::string(out), "A B C");

  sanitizeName(out, sizeof(out), "A|B;C");
  CHECK_EQ(std::string(out), "A B C");

  sanitizeName(out, sizeof(out), "A\x01\x1F\x7F" "B");
  CHECK_EQ(std::string(out), "A   B");

  sanitizeName(out, sizeof(out), "Trailing spaces   ");
  CHECK_EQ(std::string(out), "Trailing spaces");

  sanitizeName(out, sizeof(out), "");
  CHECK_EQ(std::string(out), "Bulb");

  sanitizeName(out, sizeof(out), "\"\\  ");
  CHECK_EQ(std::string(out), "Bulb");

  char small[6];
  sanitizeName(small, sizeof(small), "123456789");
  CHECK_EQ(std::string(small), "12345");
}

void runBulbRegistryTests() {
  testSanitizeName();
}

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
#include <map>
#include <vector>

class PreferencesMock {
public:
    std::map<std::string, uint8_t> uchars;
    std::map<std::string, std::vector<uint8_t>> bytes;
    std::map<std::string, String> strings;

    void begin(const char*, bool) {}

    void clear() {
        uchars.clear();
        bytes.clear();
        strings.clear();
    }

    uint8_t getUChar(const char* key, uint8_t def) {
        auto it = uchars.find(key);
        return it != uchars.end() ? it->second : def;
    }

    void putUChar(const char* key, uint8_t val) {
        uchars[key] = val;
    }

    void getBytes(const char* key, void* out, size_t len) {
        auto it = bytes.find(key);
        if (it != bytes.end() && it->second.size() == len) {
            memcpy(out, it->second.data(), len);
        }
    }

    void putBytes(const char* key, const void* in, size_t len) {
        std::vector<uint8_t> v((const uint8_t*)in, (const uint8_t*)in + len);
        bytes[key] = v;
    }

    String getString(const char* key, const char* def) {
        auto it = strings.find(key);
        return it != strings.end() ? it->second : String(def);
    }

    void putString(const char* key, const String& val) {
        strings[key] = val;
    }
};

#define Preferences PreferencesMock

#include "../bulb_registry.cpp"

void resetRegistry() {
    bulbCountValue = 0;
    stateDirty = false;
    stateChangedAtMs = 0;
    prefs.clear();
    for (size_t i = 0; i < MAX_BULBS; ++i) {
        bulbs[i] = Bulb();
    }
}

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

void testSerializeBulbState() {
  check::begin("serializeBulbState");

  BulbState st;
  st.mode = BulbColorMode::White;
  st.power = true;
  st.level = 200;
  st.kelvin = 3000;
  st.red = 255;
  st.green = 100;
  st.blue = 50;

  String serialized = serializeBulbState(st);
  CHECK_EQ(std::string(serialized.c_str()), "0,1,200,3000,255,100,50");

  BulbState out;
  CHECK(deserializeBulbState(out, serialized));
  CHECK_EQ((int)out.mode, (int)BulbColorMode::White);
  CHECK(out.power);
  CHECK_EQ(out.level, 200);
  CHECK_EQ(out.kelvin, 3000);
  CHECK_EQ(out.red, 255);
  CHECK_EQ(out.green, 100);
  CHECK_EQ(out.blue, 50);

  // Test constraint logic
  String invalid = "1,1,300,9000,300,300,300";
  CHECK(deserializeBulbState(out, invalid));
  CHECK_EQ((int)out.mode, (int)BulbColorMode::Rgb);
  CHECK(out.power);
  CHECK_EQ(out.level, 255); // Constrained to 255
  CHECK_EQ(out.kelvin, MAX_KELVIN); // Constrained to MAX_KELVIN
  CHECK_EQ(out.red, 255);
  CHECK_EQ(out.green, 255);
  CHECK_EQ(out.blue, 255);

  // Too few parts
  CHECK(!deserializeBulbState(out, "0,1,200,3000"));
}

void testRegistryLifecycle() {
  check::begin("registryLifecycle");
  resetRegistry();

  registryBegin();
  CHECK_EQ(registryCount(), (size_t)0);

  esp_zb_ieee_addr_t ieee1 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  Bulb* b1 = registryAdd(ieee1);
  CHECK(b1 != nullptr);
  CHECK_EQ(registryCount(), (size_t)1);
  CHECK_EQ(std::string(b1->name), "Bulb 1");

  esp_zb_ieee_addr_t ieee2 = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
  Bulb* b2 = registryAdd(ieee2);
  CHECK(b2 != nullptr);
  CHECK_EQ(registryCount(), (size_t)2);
  CHECK_EQ(std::string(b2->name), "Bulb 2");

  // Retrieve by IEEE
  Bulb* found1 = registryFindByIeee(ieee1);
  CHECK(found1 == b1);

  // Retrieve by hex string (remembering little endian to big endian logic)
  String hex1 = bulbIeeeHex(b1);
  CHECK_EQ(std::string(hex1.c_str()), "0807060504030201");
  Bulb* foundHex = registryFindByIeeeHex(hex1);
  CHECK(foundHex == b1);

  // Verify failure on unknown IEEE
  esp_zb_ieee_addr_t ieee3 = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  CHECK(registryFindByIeee(ieee3) == nullptr);

  // Mark dirty and flush
  registryMarkDirty();
  registryTick(); // Should not flush yet due to STATE_SAVE_DELAY_MS
  CHECK(stateDirty);

  mock_millis_registry += STATE_SAVE_DELAY_MS;
  registryTick();
  CHECK(!stateDirty);

  // Reload registry
  bulbCountValue = 0;
  for (size_t i = 0; i < MAX_BULBS; ++i) {
      bulbs[i] = Bulb();
  }
  registryBegin();

  CHECK_EQ(registryCount(), (size_t)2);
  Bulb* reloaded = registryGet(0);
  CHECK(reloaded != nullptr);
  CHECK_EQ(std::string(reloaded->name), "Bulb 1");
  CHECK(ieeeEquals(reloaded->ieee, ieee1));
}

void testRegistryLimits() {
  check::begin("registryLimitsAndRemoval");
  resetRegistry();
  registryBegin();

  // Fill up to MAX_BULBS
  for (size_t i = 0; i < MAX_BULBS; ++i) {
    esp_zb_ieee_addr_t ieee;
    memset(ieee, i + 1, sizeof(ieee));
    Bulb* b = registryAdd(ieee);
    CHECK(b != nullptr);
  }
  CHECK_EQ(registryCount(), MAX_BULBS);

  // Exceed MAX_BULBS
  esp_zb_ieee_addr_t ieeeFull = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  Bulb* fullB = registryAdd(ieeeFull);
  CHECK(fullB == nullptr);

  // Attempt duplicate add
  esp_zb_ieee_addr_t ieee1;
  memset(ieee1, 1, sizeof(ieee1));
  Bulb* dupB = registryAdd(ieee1);
  CHECK(dupB != nullptr); // Returns existing bulb
  CHECK_EQ(registryCount(), MAX_BULBS);

  // Test removing middle item (shifts remaining)
  esp_zb_ieee_addr_t ieee2;
  memset(ieee2, 2, sizeof(ieee2));
  Bulb* b2 = registryFindByIeee(ieee2);
  CHECK(b2 != nullptr);

  registryRemove(b2);
  CHECK_EQ(registryCount(), (size_t)(MAX_BULBS - 1));

  // Verify it's gone
  CHECK(registryFindByIeee(ieee2) == nullptr);

  // Test renaming
  Bulb* b1 = registryFindByIeee(ieee1);
  CHECK(b1 != nullptr);
  registryRename(b1, "Living Room Bulb");
  CHECK_EQ(std::string(b1->name), "Living Room Bulb");
}

void testBrightnessHelpers() {
  check::begin("brightnessHelpers");
  resetRegistry();
  registryBegin();

  esp_zb_ieee_addr_t ieee = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
  Bulb* b = registryAdd(ieee);
  CHECK(b != nullptr);

  // Set 50%
  bulbSetBrightnessPct(b, 50);
  CHECK_EQ(bulbBrightnessPct(b), 50);

  // Test lower bound constraint
  bulbSetBrightnessPct(b, 0);
  CHECK_EQ(bulbBrightnessPct(b), 0);

  // Test upper bound constraint
  bulbSetBrightnessPct(b, 100);
  CHECK_EQ(bulbBrightnessPct(b), 100);

  // Test out-of-bounds (over 100 should constrain to 100)
  bulbSetBrightnessPct(b, 200);
  CHECK_EQ(bulbBrightnessPct(b), 100);

  // Null pointers
  bulbSetBrightnessPct(nullptr, 50);
  CHECK_EQ(bulbBrightnessPct(nullptr), 0);
}

void runBulbRegistryTests() {
  testSanitizeName();
  testSerializeBulbState();
  testRegistryLifecycle();
  testRegistryLimits();
  testBrightnessHelpers();
}

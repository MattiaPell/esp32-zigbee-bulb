#include <Arduino.h>
#include <Preferences.h>
#include "bulb_registry.h"

extern Preferences prefs;

void setup() {
  Serial.begin(115200);
  delay(2000);

  prefs.begin(PREFS_NAMESPACE, false);
  prefs.putUChar("count", 16);
  for (size_t i = 0; i < 16; ++i) {
    char key[16];
    int len = snprintf(key, sizeof(key), "b%zu", i);
    key[len] = 'i'; key[len+1] = '\0';
    esp_zb_ieee_addr_t ieee = {0};
    prefs.putBytes(key, ieee, 8);
    key[len] = 'n'; key[len+1] = '\0';
    prefs.putString(key, "Bulb");
    key[len] = 's'; key[len+1] = '\0';
    prefs.putString(key, "0,0,128,3500,255,255,255");
    key[len] = 'a'; key[len+1] = '\0';
    prefs.putString(key, "1234,1");
  }
  prefs.end();

  uint32_t start = millis();
  for (int j = 0; j < 100; ++j) {
      registryBegin();
  }
  uint32_t end = millis();

  Serial.printf("BASELINE: registryBegin 100 iterations took %u ms\n", end - start);
}
void loop() {}

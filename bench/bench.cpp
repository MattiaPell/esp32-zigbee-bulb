#include <Arduino.h>
#include <stdio.h>
#include <time.h>
#include <Preferences.h>
#include "bulb_registry.h"

int32_t fake_millis_val = 0;
uint32_t millis() { return fake_millis_val; }

int main() {
  // Setup fake NVS with some bulbs
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  prefs.putUChar("count", 16);
  for (size_t i = 0; i < 16; ++i) {
    char key[16];
    int len = snprintf(key, sizeof(key), "b%zu", i);

    key[len] = 'i'; key[len+1] = 0;
    esp_zb_ieee_addr_t ieee = {0};
    prefs.putBytes(key, ieee, 8);

    key[len] = 'n'; key[len+1] = 0;
    prefs.putString(key, "Bulb");

    key[len] = 's'; key[len+1] = 0;
    prefs.putString(key, "0,0,128,3500,255,255,255");

    key[len] = 'a'; key[len+1] = 0;
    prefs.putString(key, "1234,1");
  }
  prefs.end();

  clock_t start = clock();
  for (int j = 0; j < 10000; ++j) {
      registryBegin();
  }
  clock_t end = clock();

  double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
  printf("registryBegin 10000 iterations took %f s\n", time_spent);
  return 0;
}

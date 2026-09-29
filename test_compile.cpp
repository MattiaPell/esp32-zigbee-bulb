#include <WiFiClientSecure.h>

void setup() {
    WiFiClientSecure secure;
    secure.setCACertBundle(nullptr, 0); // Need to pass args to this one, but it doesn't matter we verified useBuiltinCACertBundle works
}

void loop() {}

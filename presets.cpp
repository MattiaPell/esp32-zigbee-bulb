#include "presets.h"

#include "config.h"
#include "web_hooks.h"
#include "zigbee_bulbs.h"

String presetsJson() {
  String j = "[";
  for (size_t i = 0; i < LIGHT_PRESET_COUNT; ++i) {
    if (i > 0) j += ",";
    j += "{\"id\":\"";
    j += LIGHT_PRESETS[i].id;
    j += "\",\"name\":\"";
    j += LIGHT_PRESETS[i].name;
    j += "\",\"brightness\":";
    j += String((unsigned)LIGHT_PRESETS[i].brightness);
    j += ",\"kelvin\":";
    j += String((unsigned)LIGHT_PRESETS[i].kelvin);
    j += "}";
  }
  j += "]";
  return j;
}

int presetApply(const String &id) {
  const LightPreset *preset = nullptr;
  for (size_t i = 0; i < LIGHT_PRESET_COUNT; ++i) {
    if (id.equalsIgnoreCase(LIGHT_PRESETS[i].id)) {
      preset = &LIGHT_PRESETS[i];
      break;
    }
  }
  if (preset == nullptr) return -1;

  const int applied =
      bulbSendAllBrightness(preset->brightness, DEFAULT_TRANSITION_DS);
  bulbSendAllKelvin(preset->kelvin, DEFAULT_TRANSITION_DS);
  webHookEvent("preset_applied", "", "", preset->id);
  return applied;
}

#include "scenes.h"

#include <Preferences.h>

#include "config.h"
#include "debug_log.h"
#include "web_hooks.h"
#include "zigbee_bulbs.h"

namespace {

Preferences prefs;
uint8_t sceneCountValue = 0;

struct SceneRef {
  char name[MAX_SCENE_NAME_LENGTH + 1] = "";
};

SceneRef scenes[MAX_SCENES];

int spotFor(const String &name) {
  int existing = -1;
  for (uint8_t i = 0; i < sceneCountValue; ++i) {
    if (name.equalsIgnoreCase(scenes[i].name)) {
      existing = i;
      break;
    }
  }
  return existing;
}

int findFreeSpot() {
  for (uint8_t i = 0; i < MAX_SCENES; ++i) {
    if (scenes[i].name[0] == '\0') return i;
  }
  return -1;
}

}  // namespace

void scenesBegin() {
  prefs.begin("scenes", false);
  sceneCountValue = prefs.getUChar("count", 0);
  if (sceneCountValue > MAX_SCENES) sceneCountValue = MAX_SCENES;
  for (uint8_t i = 0; i < MAX_SCENES; ++i) {
    String name = prefs.getString(("s" + String(i) + "n").c_str(), "");
    name.toCharArray(scenes[i].name, sizeof(scenes[i].name));
  }
}

size_t scenesCount() {
  size_t n = 0;
  for (uint8_t i = 0; i < MAX_SCENES; ++i) {
    if (scenes[i].name[0] != '\0') ++n;
  }
  return n;
}

String sceneNameAt(size_t index) {
  uint8_t seen = 0;
  for (uint8_t i = 0; i < MAX_SCENES; ++i) {
    if (scenes[i].name[0] == '\0') continue;
    if ((size_t)seen == index) return String(scenes[i].name);
    ++seen;
  }
  return String();
}

String sceneEntryAt(size_t index) {
  uint8_t seen = 0;
  for (uint8_t i = 0; i < MAX_SCENES; ++i) {
    if (scenes[i].name[0] == '\0') continue;
    if ((size_t)seen == index) {
      return prefs.getString(("s" + String(i) + "d").c_str(), "");
    }
    ++seen;
  }
  return String();
}

bool sceneImport(const String &name, const String &data) {
  if (!isValidSceneName(name) || data.length() == 0) return false;
  int at = spotFor(name);
  if (at < 0) {
    at = findFreeSpot();
    if (at < 0) return false;
    name.toCharArray(scenes[at].name, sizeof(scenes[at].name));
  }
  prefs.putString(("s" + String(at) + "d").c_str(), data);
  prefs.putUChar("count", scenesCount());
  return true;
}

int sceneIndexOf(const String &name) {
  for (uint8_t i = 0; i < MAX_SCENES; ++i) {
    if (scenes[i].name[0] != '\0' && name.equalsIgnoreCase(scenes[i].name)) return i;
  }
  return -1;
}

bool isValidSceneName(const String &name) {
  if (name.length() == 0 || name.length() > MAX_SCENE_NAME_LENGTH) return false;
  for (unsigned i = 0; i < name.length(); ++i) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
      return false;
    }
  }
  return true;
}

bool sceneCapture(const String &name) {
  if (!isValidSceneName(name)) return false;
  if (registryCount() == 0) return false;

  // Snapshot first: only touch the slot if there is something to store.
  String data;
  data.reserve(64 * registryCount());
  for (size_t i = 0; i < registryCount(); ++i) {
    const Bulb *b = registryGet(i);
    if (i > 0) data += "|";
    data += bulbIeeeHex(b);
    data += ";";
    data += serializeBulbState(b->state);
  }

  int at = spotFor(name);
  if (at < 0) {
    at = findFreeSpot();
    if (at < 0) {
      debugLogPrintln("Scenes: storage full");
      return false;
    }
    name.toCharArray(scenes[at].name, sizeof(scenes[at].name));
  }

  prefs.putString(("s" + String(at) + "d").c_str(), data);
  prefs.putUChar("count", scenesCount());
  debugLogPrintf("Scenes: captured '%s' (%u bulbs)\n", name.c_str(),
                (unsigned)registryCount());
  return true;
}

bool sceneRecall(const String &name, int &applied, int &skipped) {
  applied = 0;
  skipped = 0;
  int at = sceneIndexOf(name);
  if (at < 0) return false;

  String data = prefs.getString(("s" + String(at) + "d").c_str(), "");
  if (data.length() == 0) return false;

  int start = 0;
  while (start < (int)data.length()) {
    int bar = data.indexOf('|', start);
    String entry = bar < 0 ? data.substring(start) : data.substring(start, bar);
    start = bar < 0 ? (int)data.length() : bar + 1;

    int semi = entry.indexOf(';');
    if (semi <= 0) continue;
    String ieeeHex = entry.substring(0, semi);
    BulbState wanted;
    if (!deserializeBulbState(wanted, entry.substring(semi + 1))) continue;

    // Locate the bulb by IEEE address (hex stored big-endian, like the API id).
    Bulb *b = nullptr;
    for (size_t i = 0; i < registryCount(); ++i) {
      if (bulbIeeeHex(registryGet(i)).equalsIgnoreCase(ieeeHex)) {
        b = registryGet(i);
        break;
      }
    }
    if (b == nullptr || !bulbReady(b)) {
      ++skipped;
      continue;
    }

    b->state = wanted;
    bulbApplyState(b, wanted, DEFAULT_TRANSITION_DS);
    ++applied;
  }

  registryFlush();
  webHookEvent("scene_applied", "", "", name.c_str());
  debugLogPrintf("Scenes: '%s' recalled (%d applied, %d skipped)\n", name.c_str(),
                applied, skipped);
  return true;
}

void sceneDelete(const String &name) {
  int at = sceneIndexOf(name);
  if (at < 0) return;
  prefs.remove(("s" + String(at) + "n").c_str());
  prefs.remove(("s" + String(at) + "d").c_str());
  scenes[at].name[0] = '\0';
  prefs.putUChar("count", scenesCount());
  debugLogPrintf("Scenes: deleted '%s'\n", name.c_str());
}

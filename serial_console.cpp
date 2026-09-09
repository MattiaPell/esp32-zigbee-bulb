#include "serial_console.h"

#include <Zigbee.h>
#include <WiFi.h>

#include "bulb_registry.h"
#include "config.h"
#include "debug_log.h"
#include "ota_update.h"
#include "web_server.h"
#include "zigbee_bulbs.h"

namespace {

bool isSpace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  list            show registered bulbs");
  Serial.println("  on <n> / off <n>  command bulb by list index (1-based)");
  Serial.println("  all off         kill switch: every bulb off");
  Serial.println("  pair            open pairing for 180 seconds");
  Serial.println("  host <name>     change mDNS hostname");
  Serial.println("  status          Wi-Fi, bulbs, memory");
  Serial.println("  version         firmware version and OTA slot");
  Serial.println("  reboot          restart the board");
  Serial.println("  reset           erase Zigbee network data and restart");
}

void printList() {
  debugLogPrintf("%u bulb(s) registered:\n", (unsigned)registryCount());
  for (size_t i = 0; i < registryCount(); ++i) {
    const Bulb *b = registryGet(i);
    debugLogPrintf(" %u. %-20s %s  addr 0x%04x ep %u\n", (unsigned)(i + 1), b->name,
                  b->online ? "online " : "offline", b->shortAddr, b->endpoint);
  }
}

void commandBulb(const String &args, bool on) {
  int n = args.toInt();
  if (n < 1 || n > (int)registryCount()) {
    debugLogPrintln("Usage: on <index from list>");
    return;
  }
  on ? bulbSendOn(registryGet(n - 1)) : bulbSendOff(registryGet(n - 1));
}

void printStatus() {
  debugLogPrintf("Wi-Fi: %s, IP %s, RSSI %d dBm\n",
                WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  debugLogPrintf("Bulbs: %u (bound %u), heap %u kB\n", (unsigned)registryCount(),
                (unsigned)zigbeeBoundDeviceCount(), (unsigned)(ESP.getFreeHeap() / 1024));
}

void handleCommand(const String &line) {
  int split = 0;
  while (split < (int)line.length() && !isSpace(line[split])) ++split;
  String cmd = line.substring(0, split);
  String args = line.substring(split);
  args.trim();

  if (cmd == "help") printHelp();
  else if (cmd == "list") printList();
  else if (cmd == "on") commandBulb(args, true);
  else if (cmd == "off") commandBulb(args, false);
  else if (cmd == "all" && args == "off") bulbSendAllOff();
  else if (cmd == "pair") zigbeeOpenPairing(PAIRING_SECONDS);
  else if (cmd == "host") {
    if (!webSetHostname(args)) {
      debugLogPrintln("Invalid name. Use a-z, 0-9, '-', 1-31 characters.");
    }
  } else if (cmd == "status") printStatus();
  else if (cmd == "version") {
    debugLogPrintf("Firmware %s, slot %s, %s\n", FW_VERSION, otaRunningSlot(),
                  otaPendingVerify() ? "pending verify" : "confirmed");
  }
  else if (cmd == "reboot") ESP.restart();
  else if (cmd == "reset") {
    debugLogPrintln("Erasing Zigbee NVRAM and restarting...");
    Zigbee.factoryReset(true);
  } else if (cmd.length() > 0) {
    debugLogPrintln("Unknown command. Type 'help'.");
  }
}

}  // namespace

void serialConsoleBegin() {
  Serial.setTimeout(100);
  debugLogPrintln("Type 'help' for serial commands.");
}

void serialConsoleTick() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() > 0) handleCommand(line);
}

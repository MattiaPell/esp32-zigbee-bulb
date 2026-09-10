# AGENTS.md — esp32-zigbee-bulb

Rules for working on this repository. The project is under active
development: the user-facing documentation lives in `README.md` and must
stay in sync with the code.

## Project

ESP32-C6 Arduino sketch (C++) acting as a standalone Zigbee coordinator for
IKEA bulbs, with an embedded web UI (PWA), scenes, off-timers and a REST
API. Arduino core for ESP32 3.3.x. 4 MB flash, dual-slot OTA layout.

## Build and verify

```sh
# Standard build (MQTT compiled out). secrets.h must exist first:
cp secrets.example.h secrets.h
arduino-cli compile \
  --fqbn "esp32:esp32:esp32c6:ZigbeeMode=zczr,PartitionScheme=custom,CDCOnBoot=default" \
  --warnings none .
```

- CI (`.github/workflows/build.yml`) runs exactly this with a secrets stub.
- Host-side unit tests (pure logic, no hardware or ESP32 toolchain):
  `bash test/run.sh` (Linux/macOS/CI, needs g++; override with
  `CXX=clang++`), or `powershell -File test/run.ps1` on Windows
  (g++/clang++/zig in PATH). CI runs them in the `host-tests` job. Keep pure
  logic in header-only files (`json_lite.h`-style) so it stays testable.
- MQTT-enabled variant: add `#define MQTT_HOST "x"` to `secrets.h` and
  rebuild — both variants must compile before merging.
- After touching `partitions.csv`, validate it:
  `python3 <arduino-esp32>/tools/gen_esp32part.py partitions.csv`
  (must parse with no overlap errors).
- Flash budget: each OTA slot is 0x1E0000 (1.875 MB); the "Custom" scheme
  does NOT enforce the size at compile time (the build prints a bogus 16 MB
  maximum). Keep the "Sketch uses" value below ~1.8 MB.

## Architecture

- One feature = one `.h`/`.cpp` module exposing plain functions; the main
  loop calls its `xBegin()` / `xTick()` pair from `esp32-zigbee-bulb.ino`.
- `web_server.cpp` owns the single `WebServer` instance, Wi-Fi/mDNS and the
  route `dispatch()` (all through `onNotFound`, except the OTA upload which
  registers its own handler). Features never touch the server directly.
- `zigbee_bulbs.cpp` owns the coordinator endpoint and all ZCL/ZDO traffic.
  Every `esp_zb_*` call is wrapped in
  `esp_zb_lock_acquire(portMAX_DELAY)` / `esp_zb_lock_release()`.
- `remote_controls.cpp`: Zigbee remotes/steering devices. Their commands are
  intercepted with `ZigbeeEP::addPrivilegeCommand` + `onPrivilegeCommand`
  (registered after `Zigbee.begin`), normalized into press events and mapped
  to actions. Enrolled from the ZDO descriptor check in `verifyTick`
  (on/off as OUTPUT cluster = remote, INPUT = light).
- `bulb_registry.cpp`: per-bulb state persisted in NVS with a debounce
  (`registryMarkDirty` + `registryTick`/`registryFlush`). Identity is the
  IEEE address; short address and endpoint are runtime-only.
- Runtime-only per-bulb data (LQI/RSSI, last-seen, command counters, group
  membership) is never persisted.
- `json_lite.h`: the shared hand-rolled JSON helpers
  (`findJsonKey`, `jsonGetBool`, `jsonGetInt`, `jsonGetString`). No JSON
  library anywhere.
- `presets.h` / `presets.cpp`: built-in quick looks (fixed brightness +
  kelvin) applied to every bulb through the collective commands. The preset
  table is header-only data; ids are URL-safe and appear in the API path.
- `effects.h` / `effects.cpp`: runtime-only lighting effects, one at a time.
  `candle` sends short-transition collective steps; `color_loop` sends the
  native ZCL command per bulb. An all-off ends the effect.
- `adaptive.h` / `adaptive.cpp`: optional circadian lighting (NTP + a fixed
  UTC offset, persisted in NVS). The curve is the pure, unit-tested
  `adaptive_curve.h`. It adjusts only powered-on white-mode bulbs and yields
  to a running effect.
- `ota_update.cpp`: upload state machine + pending-verify confirmation;
  `Update` (ESP-IDF esp_ota) with bootloader rollback
  (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`).
- `mqtt_bridge.cpp`: hand-rolled MQTT 3.1.1 client over `WiFiClient`
  (QoS 0). Compiled in only when `MQTT_HOST` is defined in `secrets.h`;
  when absent the functions are no-op stubs, so call sites never need
  `#ifdef`.
- Hostname is stored in NVS namespace `bulbs`, key `host`; read it via
  `storedHostname()` from `config.h`.

## Rules

- No third-party libraries. Core-bundled only: `WiFi`, `WebServer`,
  `Preferences`, `ESPmDNS`, `Zigbee`, `Update`, `HTTPClient`,
  `Adafruit NeoPixel`. Adding a dependency requires updating README and
  this file with a justification.
- Never commit `secrets.h`. All credentials and optional feature switches
  live there (see `secrets.example.h`); compile-time gates use
  `#if defined(...)` there, runtime toggles go to NVS + a REST endpoint.
- REST style: JSON in/out; errors always `{"error":"..."}` with a 4xx
  status; the light id is the IEEE address in lowercase hex; scene names
  are `a-z0-9_-` (max 20 chars, they appear in URL paths).
- Zigbee: never address bulbs by IEEE directly (unreliable with IKEA
  bulbs) — use short address + endpoint resolved from the binding table.
  Collective commands use the native group (id 0x0001) only when every
  reachable bulb is a confirmed member, otherwise fall back to per-bulb
  unicast. Keep sends staggered (one command per tick pass where a queue
  exists).
- Do not write NVS while an OTA flash is running (`registryFlush()` before
  `Update.begin()`; the loop skips the other ticks while uploading).
- The embedded web page (`web_page.h`, Italian UI strings) must be updated
  when a new API surface or user-visible behavior lands.
- README is updated in the same change whenever endpoints, secrets,
  settings or user-visible behavior change.
- Commits: short imperative subject ("Add X", "Fix Y"), one theme per
  commit. Docs-only changes can be a separate commit.

## Gotchas

- Partition layout changes move the Zigbee storage partitions: the network
  is rebuilt and bulbs must be re-paired once. NVS (bulb registry, scenes,
  hostname) keeps its offset and survives.
- MQTT publishes and webhook POSTs run on the main loop (no task
  offloading): keep their timeouts small; one in-flight request at a time.
- `WebServer` is single-threaded: during a multipart OTA upload no other
  request is served; don't add long-running work inside handlers.
- The OTA "pending verify" window is time-based (90 s): a reboot inside it
  rolls back automatically, so never restart the board gratuitously in
  early-boot code paths.

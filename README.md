# esp32-zigbee-bulb

[![build](https://github.com/MattiaPell/esp32-zigbee-bulb/actions/workflows/build.yml/badge.svg)](https://github.com/MattiaPell/esp32-zigbee-bulb/actions/workflows/build.yml)

> ⚠️ **Work in progress** — this project is under active development and may
> contain bugs or change without notice. Do not rely on it for anything
> critical, and read the release notes before updating a working install.
> See [CHANGELOG.md](CHANGELOG.md) for what changed.

A standalone web controller for IKEA Zigbee bulbs, built on an ESP32-C6.
The board forms its own Zigbee network, pairs any number of bulbs, and
serves a control page over Wi-Fi — no hub, no cloud, no app.

Features:

- **Multi-bulb**: pairs and controls several bulbs, each with its own card
  in the web page (rename, remove, per-bulb state);
- collective commands travel as **native Zigbee group frames** (one message
  for all bulbs) with automatic per-bulb fallback;
- on/off, brightness, white temperature (2200–4000 K) and RGB color per bulb;
- **named scenes**: capture the current state of every bulb under a name
  and recall it later (persisted in NVS);
- **quick presets**: built-in one-tap looks (relax / focus / notte) that set
  every bulb to a fixed brightness and white temperature;
- **effects**: a native ZCL color loop on color bulbs, and a warm "candle"
  flicker driven by short collective transitions;
- **adaptive lighting** (optional): with NTP and a UTC offset, white
  temperature and brightness follow a circadian curve;
- **off timers**: schedule any (or all) bulbs to turn off in a few minutes;
- **installable app**: the control page is a PWA with its own icon —
  "Add to home screen" and it runs fullscreen like a native app;
- light/dark theme and drag-to-reorder bulb cards (the order is kept in the
  browser);
- light state (power, brightness, white tone or color) restored after a
  reboot or power cut, then reconciled with what the bulbs actually report;
- automatic pairing while no bulb is bound, or on demand from the UI;
- a BOOT-button kill switch that turns every bulb off;
- REST API and a lightweight web UI (served from flash, no external assets);
- per-bulb **link diagnostics**: LQI/RSSI, last-seen and command failure
  counters (REST API and settings dialog);
- **webhook events**: `boot`, `bulb_online`, `bulb_offline`, `timer_expired`
  and `scene_applied` posted as JSON to external URLs;
- **Zigbee remote controls** (TRÅDFRI, STYRBAR…): pair one and its buttons
  drive the lights out of the box (toggle, dim, scene cycling), with a
  configurable action map;
- optional **MQTT bridge** for Home Assistant (hand-rolled client; compiled
  in only when a broker host is configured in `secrets.h`);
- configurable mDNS hostname, so several controllers can share one network;
- **OTA updates**: firmware upload over HTTP into a second slot, with
  automatic rollback if the new firmware fails to boot;
- status LED (Wi-Fi / Zigbee / pairing / ready) and a serial console.

Deliberately out of scope: alarms and wake-light scheduling.

## Hardware

- ESP32-C6 development board with 4 MB flash
- IKEA Zigbee bulbs — dimming and white temperature work on any IKEA bulb;
  RGB color requires a color model such as TRÅDFRI E27 CWS 806 lm or
  SOLHETTA E27 CWS 1000 lm
- USB cable for flashing

The sketch assumes the board's NeoPixel is on GPIO 8 and the BOOT button is
on GPIO 9. Change `STATUS_PIXEL_PIN` / `POWER_BUTTON_PIN` in `config.h` for
other boards.

## Software

- Arduino IDE 2 (or arduino-cli)
- Espressif Arduino core for ESP32, version 3.3 or newer
- Adafruit NeoPixel library

No other dependencies: the web page is embedded, JSON is hand-rolled, the
MQTT client is hand-rolled too (the only extra core library in use is
HTTPClient, shipped with the ESP32 Arduino core, for webhooks).

## Tests

Pure logic (the hand-rolled JSON reader in `json_lite.h`, plus other
header-only helpers) is covered by host-side unit tests under `test/` — no
ESP32 toolchain, no hardware, just a C++17 compiler:

```sh
bash test/run.sh                 # Linux/macOS/CI (g++ by default; CXX=... to change)
powershell -File test/run.ps1    # Windows (g++, clang++ or zig in PATH)
```

CI runs them on every push and pull request (the `host-tests` job).

## Configure and flash

1. Open `esp32-zigbee-bulb.ino` in the Arduino IDE.
2. Copy `secrets.example.h` to `secrets.h` and enter your 2.4 GHz Wi-Fi
   credentials. (`secrets.h` is ignored by Git and must not be committed.)
3. Select `ESP32C6 Dev Module` under **Tools > Board**.
4. `USB CDC On Boot` → **Disabled** (the serial console runs on the
   board's UART bridge; if you cable the ESP32-C6's USB-JTAG connector
   instead, set it to Enabled).
5. `Zigbee mode` → **Zigbee ZCZR (coordinator/router)**.
6. `Partition Scheme` → **Custom** (the included `partitions.csv` keeps
   Espressif's Zigbee storage partitions and provides two OTA slots).
7. Upload, then open the serial monitor at 115200 baud.

The custom 4 MB layout has two OTA application slots (1.875 MB each) next
to Espressif's Zigbee storage partitions, so firmware updates also work
over Wi-Fi (see "OTA updates" below).

## OTA updates

`POST /api/ota` accepts a multipart firmware upload and writes it to the
inactive slot, then reboots into it:

```
curl -F "update=@esp32-zigbee-bulb.ino.bin" http://bulb.local/api/ota
```

- Optional checksum: send the MD5 of the `.bin` in the `X-OTA-MD5` header;
  the upload is rejected if the written image does not match.
- Optional token: define `OTA_TOKEN` in `secrets.h` to require the same
  value in the `X-OTA-TOKEN` header on every upload.
- Rollback: the new slot boots as "pending verify". If the board restarts
  within 90 seconds (crash or boot loop), the bootloader switches back to
  the previous slot automatically. After 90 seconds of stable operation the
  image is confirmed valid.
- The endpoint is meant for a trusted LAN only; do not expose it to the
  internet.
- Headroom: each slot is 1.875 MB against a current firmware of ~1.7 MB.
  The "Custom" partition scheme does not enforce the size at compile time,
  so watch the "Sketch uses" line of the build output and keep the binary
  below ~1.8 MB.

`GET /api/status` reports the running slot (`ota.slot`) and whether the
image is still pending verification (`ota.pending_verify`); the serial
console shows the same with `version`.

Upgrading from a firmware older than the dual-slot layout moves the Zigbee
storage partitions, so the Zigbee network is rebuilt and bulbs must be
re-paired once. The stored registry (names, states, scenes, hostname)
survives, because the NVS partition keeps its offset.

## Pair a bulb

1. Keep the bulb close to the board.
2. With no bulb paired, the network stays open automatically. You can also
   press **Aggiungi lampadina** in the web page (or `pair` on the serial
   console) to open a 3-minute pairing window.
3. Reset the bulb by switching its power off and on six times, ending with
   the bulb powered on. It joins and appears in the page within seconds.
4. Open the address printed on the serial monitor, or `http://bulb.local/`
   (the hostname is configurable).

To remove a bulb, use the ✕ on its card: the controller sends a ZDO unbind
and forgets the stored state.

## Remote controls

IKEA Zigbee remotes and steering devices (TRÅDFRI remote, STYRBAR,
SYMFONISK dial…) can drive the lights directly:

1. Open a pairing window (**Aggiungi lampadina**, or `pair` on the serial
   console).
2. Put the remote in pairing mode per the IKEA instructions (usually a
   long press of the pairing/reset button, close to the board).
3. The remote binds to the controller and appears under
   **Impostazioni → Telecomandi** (or `GET /api/remotes`). It is
   registered even before the first press.

Default behavior (works with zero configuration):

| Press | Action |
|---|---|
| center: toggle | master switch: all bulbs on / off |
| ring up / down (held) | brightness of every bulb ±10 % |
| on / off keys | all bulbs on / off |
| arrows / temp wheel steps | previous / next scene (cycling) |
| scene recall (if sent) | recalls the scene with that number |

Customize with `POST /api/remotes/actions`, e.g.:

```
curl -X POST http://bulb.local/api/remotes/actions \
     -H 'Content-Type: application/json' \
     -d '{"toggle":"scene_next","move_up":"all_on","stop":"all_off"}'
```

Events: `off`, `on`, `toggle`, `move_up`, `move_down`, `stop`, `step_up`,
`step_down`, `color_a`, `color_b` (fixed at `GET /api/remotes/actions`;
scene recall always recalls the scene in the payload). Actions: `none`,
`all_on`, `all_off`, `toggle_all`, `brightness_up`, `brightness_down`,
`scene_next`, `scene_prev`. Every press also fires a `remote_pressed`
webhook. Presses of commands outside the set above are logged on the serial
console (see `GET /api/remotes` for the last one).

Remotes work over unicast bindings (how they pair with the coordinator): a
remote configured to send to a Zigbee group is not captured. Renames are
per-remote; the action map is global. To re-enroll a forgotten remote,
factory-reset it and pair again.

### Direct steering (remote → bulb, without the coordinator)

After a factory reset a remote joins the network with no bindings, so its
keys go nowhere. Two ways to fix that:

1. **Coordinator relay (default)**: do nothing — presses arrive at the
   coordinator and drive the default action map above.
2. **Direct link**: bind the remote to a specific bulb so it steers it even
   if the board is off. In **Impostazioni → Telecomandi** choose the bulb
   and press **Collega alla lampadina** (or:

```
curl -X POST http://bulb.local/api/remotes/{id}/bind \
     -H 'Content-Type: application/json' -d '{"light":"<bulb id>"}'
```

The coordinator sends ZDO Bind requests (on/off, level, color control)
staggered over a few seconds; press a key on the remote to wake it so it
accepts them. The web card keeps updating via state readback.

The onboard status LED shows: blinking blue = Wi-Fi connecting, blinking
purple = Zigbee starting, breathing amber = pairing window open / no bulbs,
solid green = ready, fast red = startup error.

## REST API

All requests and responses are JSON. The light id is the bulb's IEEE
address in hex (stable across reboots).

| Method | Path                | Description |
|--------|---------------------|-------------|
| GET    | `/api/lights`       | list of bulbs with current state (`?debug=1` adds a `debug` object per bulb: `lqi`, `rssi`, `last_seen_s` (-1 = never), `cmd_sent`, `cmd_failed`, `last_fail`) |
| PATCH  | `/api/lights`       | collection update: `{"on":true}` turns every reachable bulb on (each at its own last state), `{"on":false}` all off; also `brightness`, `kelvin`, `rgb_hex` applied to every reachable bulb as one group frame |
| PATCH  | `/api/lights/{id}`  | partial update: `name`, `on`, `brightness` (0–100), `mode` (`white`/`rgb`), `kelvin`, `rgb_hex` (`"#ff8800"`), optional `transition` (0.1 s units) |
| DELETE | `/api/lights/{id}`  | unbind and forget a bulb |
| GET    | `/api/lights/{id}/debug` | per-bulb diagnostics: link quality, last seen, command counters |
| GET    | `/api/pairing`      | `{"open":false,"seconds":180}` |
| POST   | `/api/pairing`      | `{"seconds":180}` opens the network |
| GET    | `/api/status`       | version, uptime, IP, RSSI, heap, bulb count, OTA slot and pending-verify state |
| GET    | `/api/devices`      | live Zigbee network snapshot: every known device with its `ieee`, `short` address, `type` (`coordinator`/`router`/`end-device`/`bound`) and name when registered |
| GET    | `/api/logs`         | in-RAM event log (last 100 lines: pairing, Zigbee commands, OTA, MQTT, webhooks); `?since=<index>` returns only newer lines; response `{"first":..,"lines":[{"i":..,"t":ms,"m":"text"},...]}`; lost on reboot |
| DELETE | `/api/logs`         | clears the log buffer (line indexes keep increasing) |
| POST   | `/api/ota`          | multipart firmware upload (`update=@file.bin`); optional `X-OTA-MD5` / `X-OTA-TOKEN` headers; reboots on success |
| GET    | `/api/hooks`        | configured webhook URLs |
| POST   | `/api/hooks`        | `{"url":"https://..."}` adds a webhook target (max 3) |
| DELETE | `/api/hooks`        | `{"url":"https://..."}` removes it |
| POST   | `/api/hooks/test`   | `{"url":"https://..."}` sends a test event to that URL (not saved) |
| GET    | `/api/mqtt`         | `{"enabled":..,"connected":..,"host":..,"port":..}` (only when MQTT is compiled in) |
| POST   | `/api/mqtt`         | `{"enabled":true|false}` runtime toggle (persisted) |
| GET    | `/api/remotes`      | paired Zigbee remotes/steering devices with the last press |
| PATCH  | `/api/remotes/{id}` | `{"name":"..."}` renames a remote |
| DELETE | `/api/remotes/{id}` | forgets a remote (factory-reset the remote to re-enroll) |
| POST   | `/api/remotes/{id}/bind` | `{"light":"<bulb id>"}` queues ZDO Bind requests for direct steering (remote → bulb) |
| GET    | `/api/remotes/actions` | the event → action map |
| POST   | `/api/remotes/actions` | `{"toggle":"toggle_all","step_up":"brightness_up",...}` overrides the map |
| GET    | `/api/backup`       | full configuration export as one JSON document |
| POST   | `/api/restore`      | re-import the same document (see "Backup and restore") |
| POST   | `/api/hostname`     | `{"hostname":"my-light"}` (a-z, 0-9, `-`) |
| GET    | `/api/scenes`       | list of saved scenes |
| POST   | `/api/scenes`       | `{"name":"relax"}` captures every bulb's current state |
| PATCH  | `/api/scenes/{name}`| re-captures (updates) an existing scene |
| POST   | `/api/scenes/{name}/recall` | applies the stored states |
| DELETE | `/api/scenes/{name}` | forgets a scene |
| GET    | `/api/presets`      | built-in quick looks (`relax`, `focus`, `notte`) with their brightness and kelvin |
| POST   | `/api/presets/{id}` | applies a preset to every reachable bulb |
| GET    | `/api/effects`      | `{"active":..,"effects":["candle","color_loop"]}` |
| POST   | `/api/effects`      | `{"effect":"candle"}` / `{"effect":"color_loop"}` / `{"effect":"none"}` starts or stops an effect |
| GET    | `/api/adaptive`     | adaptive state: `enabled`, `synced`, `tz_offset_min`, and (once synced) `local`, `kelvin`, `brightness` |
| POST   | `/api/adaptive`     | `{"enabled":true}` and/or `{"tz_offset_min":120}` configures adaptive lighting |
| POST   | `/api/lights/{id}/timer` | `{"seconds":600}` turns that bulb off after the delay (0 cancels) |
| POST   | `/api/timer`        | same, for every bulb (the deadline lives only in RAM: a reboot clears it) |

Examples:

```
curl http://bulb.local/api/lights
curl -X PATCH http://bulb.local/api/lights/a4c138d0e0b12c34 \
     -H 'Content-Type: application/json' \
     -d '{"brightness":40,"kelvin":2700}'
curl -X PATCH http://bulb.local/api/lights/a4c138d0e0b12c34 \
     -H 'Content-Type: application/json' -d '{"rgb_hex":"#ff8800"}'
```

Errors are always `{"error":"..."}` with a 4xx status.

Scene names are restricted to lowercase letters, digits, `-` and `_`
(max 20 characters) because they appear in URL paths. Recalling a scene
skips bulbs that are offline or were not part of the scene (e.g. paired
after it was captured).

Effects run on the coordinator and are runtime-only (a reboot clears them,
like the off-timers). `color_loop` needs color bulbs; `candle` fades every
reachable bulb through warm white between 2200 K and 2600 K. Turning all the
bulbs off ends the active effect.

Adaptive lighting needs a working NTP path (Wi-Fi plus internet). It only
adjusts bulbs that are on and in white mode (RGB scenes are left alone) and
pauses while an effect runs; the UTC offset is fixed (no automatic DST), so
adjust it when the clocks change.

## Serial console

`help`, `list`, `on <n>` / `off <n>`, `all off`, `pair`, `host <name>`,
`status`, `version`, `reboot`, `reset` (erases the Zigbee network and
reboots).

## How it works

The controller exposes a `ZigbeeColorDimmerSwitch` endpoint with
multi-binding enabled, so every bulb that joins gets bound to it. Bulbs are
addressed by short address + endpoint, both refreshed at runtime: after a
boot the short addresses are re-resolved from IEEE addresses (a ZDO
NWK-address request per bulb), and attribute reports are routed back to the
right bulb by source address. Direct IEEE-addressed commands are avoided on
purpose — they proved unreliable with IKEA bulbs.

Per-bulb state is persisted in NVS and written with a debounce. Identity is
the IEEE address, so bulbs keep their name and state across reboots even if
their short address changes.

Collective updates (PATCH `/api/lights` and the kill switch) use a native
Zigbee **group** (id 0x0001): every registered bulb is enrolled into the
group with an idempotent Add Group command (re-sent whenever a bulb comes
online, since bulbs store the membership themselves), so one group-addressed
frame commands all of them at once. Whenever a reachable bulb is not a
confirmed member yet, the command falls back to one frame per bulb — same
result, more latency. Remove a bulb and it is told to leave the group.

## Webhook events

The controller can POST a JSON body to up to three external URLs on these
events: `boot`, `bulb_joined`, `bulb_removed`, `bulb_online`, `bulb_offline`,
`timer_expired`, `scene_applied`, `preset_applied`, `remote_pressed`,
`remote_bind_result`
(`detail` = `queued` / `armed`), `ota_success`, `ota_failed` and
`ota_rejected` (and `test`, via `/api/hooks/test`). Example body:

```json
{"event":"bulb_offline","bulb":"a4c138d0e0b12c34","name":"Salotto",
 "detail":"","uptime_s":1234,"version":"1.0.0"}
```

- URLs are managed with `/api/hooks` (persisted); alternatively uncomment
  `WEBHOOK_URL_1` in `secrets.h` to seed the first one.
- Each URL gets its own POST; a failing one is retried up to 3 times with a
  1 s / 5 s / 30 s backoff, then the event moves on and is eventually
  dropped. Nothing is sent while Wi-Fi is down.
- `https://` URLs are accepted, but the server certificate is **not**
  verified (no CA store on the device): use them only on trusted networks.
- Each POST blocks the main loop for up to ~2 s (no offloading available),
  so point the hooks at fast, local receivers.

## MQTT (optional)

A minimal MQTT 3.1.1 client (hand-rolled over plain TCP, QoS 0, no TLS)
publishes bulb state and accepts commands, aimed at Home Assistant. It is
compiled **only** when `MQTT_HOST` is defined in `secrets.h`; without it the
feature does not exist in the firmware. A runtime toggle survives in NVS.

```c
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883        // optional, default 1883 (plain TCP only)
#define MQTT_USER "homeassistant"   // optional
#define MQTT_PASS "..."             // optional
#define MQTT_PREFIX "bulbctl"       // optional, default "bulbctl"
#define MQTT_DISCOVERY 1            // optional, default 1 (HA discovery)
```

Topics (id = bulb IEEE address in hex):

| Topic | Content |
|---|---|
| `bulbctl/bridge` | retained `online`/`offline`, also the LWT of the bridge |
| `bulbctl/<id>/state` | retained `{"state":"ON","brightness":128,"color_mode":"color_temp","color_temp":370}` (or `"color":{"r":..,"g":..,"b":..}`) |
| `bulbctl/<id>/availability` | retained `online`/`offline` per bulb |
| `bulbctl/<id>/set` | commands: `{"state":"ON","brightness":128,"color_temp":370,"color":{"r":255,"g":0,"b":0}}` (fields are optional) |
| `bulbctl/all/set` | same, applied to every bulb as one collective command |
| `bulbctl/<id>/lqi` | retained link quality (0–255) |
| `bulbctl/<id>/rssi` | retained RSSI (dBm) |
| `bulbctl/<id>/last_seen` | retained seconds since the bulb last reported |

With `MQTT_DISCOVERY=1` each bulb publishes a retained Home Assistant
discovery config on
`homeassistant/light/bulbctl-<hostname>-<id>/light/config`, so the lights
appear automatically (brightness, color temperature and RGB as reported by
the bulb). The controller itself is exposed as a connectivity binary_sensor
(also the `via_device` of every bulb), and each bulb gets three diagnostic
sensors (LQI, RSSI, seconds since last seen). Delete a bulb and its
discovery entry goes stale: clear it from HA, or set `MQTT_DISCOVERY=0` and
use the topics manually.

`GET`/`POST /api/mqtt` toggles the bridge at runtime. Limitations: QoS 0
only (no PUBACK tracking), plain TCP (put the broker on the LAN, or use a
TLS-terminating local forwarder if you need encryption), state changes are
pushed rate-limited (max one bulb per 100 ms).

## Backup and restore

`GET /api/backup` exports the configuration as one JSON document:

```json
{"version":1,"hostname":"bulb",
 "bulbs":[{"ieee":"a4c1...","name":"Salotto","state":"0,1,128,2700,255,255,255"}],
 "scenes":[{"name":"relax","bulbs":[{"ieee":"a4c1...","state":"0,1,200,3000,255,255,255"}]}],
 "hooks":{"urls":["https://..."]},
 "mqtt":{"enabled":true}}
```

`POST /api/restore` re-imports it after a clean flash (a text editor round
trip works too: bulb names, scenes, hostname, webhook URLs and the MQTT
toggle are all rewritten; unknown or oversized entries are skipped and
counted in the response).

After a full reflash the Zigbee network is new, so the bulbs must re-join
one by one — but restore **before** re-pairing: when a bulb with a restored
IEEE address joins, it comes back already named, with its stored state and
its scene memberships. To push the restored states to the bulbs, recall a
scene or send commands afterwards.

## Known limitations

- **Remotes bound to a Zigbee group are not captured.** Button presses are
  intercepted only when the remote sends them to the coordinator endpoint
  (the usual pairing). A remote already configured to address a group keeps
  steering those bulbs, but the coordinator sees nothing: the action map and
  `remote_pressed` do not fire. Re-pair the remote (factory reset) to use the
  coordinator relay, or bind it directly to a bulb from the settings dialog.
- **Off-timers and effects are runtime-only.** They live in RAM and are lost
  on reboot or power cut (a power cut must not turn lights off hours later).
- **Adaptive lighting uses a fixed UTC offset** (no automatic DST), so adjust
  it when the clocks change; it needs a working NTP path.
- **`candle` is a coordinator-side simulation**, not a bulb-native effect: it
  steps brightness and white temperature with short transitions.
  `color_loop` uses the native ZCL command and needs color bulbs.
- **MQTT is QoS 0 over plain TCP** (no TLS), and `https://` webhook targets
  are not certificate-verified (no CA store on the device): keep both on a
  trusted LAN.
- **The web server is single-threaded**: during a multipart OTA upload no
  other request is served, and long handlers block the loop.
- **`POST /api/ota` is for a trusted LAN only** (define `OTA_TOKEN` in
  `secrets.h` to require a token).

## Roadmap

- Capture group-addressed remote presses (needs a Groups server cluster on
  the coordinator endpoint and testing on real hardware).
- DST-aware time zones and per-day overrides for adaptive lighting.
- Surface the effect list in the serial console.

## Credits

- [Espressif Arduino core](https://github.com/espressif/arduino-esp32) — the
  Zigbee library and its examples, under the Apache License 2.0, are the
  foundation of the coordinator setup here.
- [Daniel90mm/esp32c6-zigbee-ikea](https://github.com/Daniel90mm/esp32c6-zigbee-ikea) —
  the project that inspired this one. This repository is an independent
  implementation: no code is shared, but the idea (an ESP32-C6 controller
  for IKEA bulbs with a web page) and the pairing flow come from it.

## License

[MIT](LICENSE) — © 2026 MattiaPell

# esp32-zigbee-bulb

A standalone web controller for IKEA Zigbee bulbs, built on an ESP32-C6.
The board forms its own Zigbee network, pairs any number of bulbs, and
serves a control page over Wi-Fi — no hub, no cloud, no app.

Features:

- **Multi-bulb**: pairs and controls several bulbs, each with its own card
  in the web page (rename, remove, per-bulb state);
- on/off, brightness, white temperature (2200–4000 K) and RGB color per bulb;
- light state (power, brightness, white tone or color) restored after a
  reboot or power cut, then reconciled with what the bulbs actually report;
- automatic pairing while no bulb is bound, or on demand from the UI;
- a BOOT-button kill switch that turns every bulb off;
- REST API and a lightweight web UI (served from flash, no external assets);
- configurable mDNS hostname, so several controllers can share one network;
- status LED (Wi-Fi / Zigbee / pairing / ready) and a serial console.

Planned for a later phase: named scenes. Deliberately out of scope: alarms
and wake-light scheduling.

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

No other dependencies: the web page is embedded, JSON is hand-rolled.

## Configure and flash

1. Open `esp32-zigbee-bulb.ino` in the Arduino IDE.
2. Copy `secrets.example.h` to `secrets.h` and enter your 2.4 GHz Wi-Fi
   credentials. (`secrets.h` is ignored by Git and must not be committed.)
3. Select `ESP32C6 Dev Module` under **Tools > Board**.
4. `USB CDC On Boot` → **Enabled**.
5. `Zigbee mode` → **Zigbee ZCZR (coordinator/router)**.
6. `Partition Scheme` → **Custom** (the included `partitions.csv` keeps
   Espressif's Zigbee storage partitions and gives the app room to breathe).
7. Upload, then open the serial monitor at 115200 baud.

The custom 4 MB layout has one large application partition and no OTA slot;
firmware updates are flashed over USB.

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

The onboard status LED shows: blinking blue = Wi-Fi connecting, blinking
purple = Zigbee starting, breathing amber = pairing window open / no bulbs,
solid green = ready, fast red = startup error.

## REST API

All requests and responses are JSON. The light id is the bulb's IEEE
address in hex (stable across reboots).

| Method | Path                | Description |
|--------|---------------------|-------------|
| GET    | `/api/lights`       | list of bulbs with current state |
| PATCH  | `/api/lights/{id}`  | partial update: `name`, `on`, `brightness` (0–100), `mode` (`white`/`rgb`), `kelvin`, `rgb_hex` (`"#ff8800"`), optional `transition` (0.1 s units) |
| DELETE | `/api/lights/{id}`  | unbind and forget a bulb |
| GET    | `/api/pairing`      | `{"open":false,"seconds":180}` |
| POST   | `/api/pairing`      | `{"seconds":180}` opens the network |
| GET    | `/api/status`       | version, uptime, IP, RSSI, heap, bulb count |
| POST   | `/api/hostname`     | `{"hostname":"my-light"}` (a-z, 0-9, `-`) |

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

## Serial console

`help`, `list`, `on <n>` / `off <n>`, `all off`, `pair`, `host <name>`,
`status`, `reboot`, `reset` (erases the Zigbee network and reboots).

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

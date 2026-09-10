# Changelog

All notable changes to this project are documented here. The format is based
on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Host-side unit tests (`test/`) for the pure logic (`json_lite.h`, the preset
  table, the adaptive curve) plus a `host-tests` CI job. Run them with
  `bash test/run.sh` (or `powershell -File test/run.ps1`).
- New webhook events: `bulb_joined`, `bulb_removed`, `remote_bind_result`,
  `preset_applied`, `ota_success`, `ota_failed`, `ota_rejected`.
- Quick presets (Relax / Focus / Notte) applied to every bulb, with
  `GET /api/presets` and `POST /api/presets/{id}` and UI buttons.
- Effects: a warm `candle` flicker and the native ZCL `color_loop`, with
  `GET/POST /api/effects` and UI buttons (one effect at a time, runtime-only).
- Adaptive lighting: white temperature and brightness follow a circadian
  curve using NTP and a fixed UTC offset, with `GET/POST /api/adaptive` and a
  settings toggle.
- MQTT: the controller is published as a Home Assistant device (a
  connectivity binary_sensor and the `via_device` of every bulb) and each
  bulb gets LQI / RSSI / last-seen diagnostic sensors; the light discovery now
  uses `availability_mode: all` and an explicit `brightness_scale`.
- Web UI: light/dark theme (remembered per browser) and drag-to-reorder bulb
  cards.

### Fixed

- `json_lite.h`: a string value that happens to equal the name of a later key
  (e.g. a bulb named `"power"`) no longer shadows the real key.

### Security

- (unchanged) The REST API has no authentication: keep the controller on a
  trusted LAN.

## [1.1.0] - 2026-09-10

### Added

- Zigbee remote controls (TRÅDFRI, STYRBAR…) as triggers: button presses are
  intercepted, normalized into press events, mapped to actions (toggle, dim,
  scenes) and can be bound directly to a bulb for coordinator-independent
  steering.
- In-RAM debug log with a web Log tab and `/api/logs`.
- `transition` parameter on the light endpoints.
- `/api/devices` documented in the REST table.

### Fixed

- Coordinator crash while binding a sleepy remote.
- A busy binding now reports busy instead of a misleading "unknown remote"
  error.
- Unterminated nested `ota` object in the `/api/status` output.

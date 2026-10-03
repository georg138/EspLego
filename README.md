# EspLego

An ESPHome device that remote-controls and automates multiple LEGO Powered Up
train hubs from a single ESP32, over Bluetooth LE — with Home Assistant
integration, a standalone local web UI, and a physical speed knob, all
running from AA batteries.

## Why this exists

[`lemio/esp32_PoweredUp`](https://github.com/lemio/esp32_PoweredUp) (the
project this started from) is a standalone Arduino library with its own
bundled BLE stack — not an ESPHome component, and not usable alongside
ESPHome's own BLE stack in one firmware. So this repo implements the LEGO
Wireless Protocol v3 (LWP3) directly as a custom ESPHome `external_component`
(`external_components/lwp3_hub/`), built on ESPHome's native `ble_client`,
which already supports multiple simultaneous BLE connections from one ESP32.
See [`PLAN.md`](PLAN.md) for the full architecture writeup and history.

## What it does

- Drives each hub's motor port(s) as an ESPHome `number` entity (−100..100),
  auto-discovered via the hub's own Hub Attached I/O announcement.
- Per-hub minimum-power deadband (`min_power:`) so a motor that needs a
  boost to overcome static friction doesn't just hum in place at low power.
- Duplo Train Base built-in speaker support (5 predefined sounds as
  `button` entities).
- A physical KY-040 rotary encoder + button: turn to set speed, press to
  stop — works even if Home Assistant/Wi-Fi is down.
- A global "Stop All Trains" safety switch, and automatic motor zeroing on
  BLE disconnect.
- Per-hub BLE enable/disable switch (park a hub without power-cycling it).
- A local web UI (`http://<device-ip>/`) with no Home Assistant dependency.
- Battery operation (4×AA, no physical power switch): deep sleep after an
  inactivity timeout, woken back up by the knob's button, with status LEDs
  for each hub's connection state and awake/asleep.

## Hardware

- Generic ESP32 DevKitC (`esp32dev`, esp-idf framework).
- LEGO Powered Up hubs — currently wired: Hub 1 (Duplo Train Base), Hub 2 &
  Hub 3 (plain Powered Up Train Hub). A 4th hub slot exists in the full
  config, pending its MAC.
- A KY-040 rotary encoder with integrated pushbutton.
- 4 LEDs (one per connected hub + one "awake" indicator).
- 4×AA batteries into the board's 5V/VIN pin.

See [`docs/gpio_pinout.md`](docs/gpio_pinout.md) for the full pin map,
including reserved pins for 3 more knobs.

## Repo layout

```
esphome/
  common/              shared base.yaml (wifi/api/ota/web_server), ble_base.yaml, safety.yaml
  hub-package.yaml      reusable per-hub block for the full 4-hub config
  train-controller.yaml        the full 4-hub build
  train-controller-dev.yaml    single-board bring-up config (3 hubs, knob, LEDs, deep sleep - the active dev/test config)
  scan_hubs.yaml         throwaway tool to find a hub's BLE MAC
  secrets.yaml.example   template - copy to secrets.yaml and fill in
external_components/lwp3_hub/   the custom LWP3 BLE component (protocol, hub lifecycle, number/button platforms)
docs/                  BLE MAC scanning guide, GPIO pin plan
PLAN.md                full build history and architecture rationale
```

## Setup

1. `cd esphome && cp secrets.yaml.example secrets.yaml`, fill in your Wi-Fi
   credentials and an API encryption key (`openssl rand -base64 32`).
2. Find each hub's BLE MAC: flash `scan_hubs.yaml`, power hubs on, read the
   logs (see [`docs/ble_mac_scan.md`](docs/ble_mac_scan.md)). Add them to
   `secrets.yaml`.
3. `esphome run train-controller-dev.yaml` to bring up the dev board, or
   `train-controller.yaml` for the full 4-hub build.

Requires Python 3 + `pip install esphome`. If you hit `CERTIFICATE_VERIFY_FAILED`
during compile, set `SSL_CERT_FILE`/`REQUESTS_CA_BUNDLE` to your certifi bundle
(`python3 -m certifi`).

## License

MIT, except the C++ files under `external_components/` which are GPLv3
(they derive from ESPHome's own GPLv3-licensed C++ runtime). See
[`LICENSE`](LICENSE).

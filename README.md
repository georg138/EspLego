# EspLego

An ESPHome device that remote-controls and automates multiple LEGO Powered Up
train hubs from a single ESP32, over Bluetooth LE — with Home Assistant
integration, a standalone local web UI, a physical speed knob, and
battery-power support via deep sleep.

## Why this exists

[`lemio/esp32_PoweredUp`](https://github.com/lemio/esp32_PoweredUp) (the
project this started from) is a standalone Arduino library with its own
bundled BLE stack — not an ESPHome component, and not usable alongside
ESPHome's own BLE stack in one firmware. So this repo implements the LEGO
Wireless Protocol v3 (LWP3) directly as a custom ESPHome `external_component`
(`external_components/lwp3_hub/`), built on ESPHome's native `ble_client`,
which already supports multiple simultaneous BLE connections from one ESP32.
See [`PLAN.md`](PLAN.md) for the full architecture writeup and history.

## Current status

**Confirmed working on real hardware:**
- Hub 1 (Duplo Train Base) motor control, including the `min_power` boost.
- Knob 1 (rotary encoder + button), driving speed/stop (before the 4-knob rework).
- BLE MAC scanning workflow (`scan_hubs.yaml`).

**Implemented and compiles, but not yet verified on real hardware:**
- **Battery operation (4×AA) has not been tested at all** — the board has
  only run on USB power so far. Deep sleep and the wake-from-button cycle
  are implemented but unverified end-to-end.
- Hub 2 and Hub 3 motor control (both connect and report their port
  topology correctly, but actual motor movement hasn't been explicitly
  confirmed for either).
- Duplo speaker sound buttons (an activation-step fix was applied after an
  initial "no sound" report; not re-tested since).
- The local web UI and per-hub BLE enable/disable switch.

**Known gaps:**
- **Knobs 2-4 are in the dev config but not wired yet.** Knob N drives hub
  N only (Knob 1 is no longer a master throttle). Pins and the active-high
  wiring (module `+` to GND, GND to 3.3V) are in [`docs/gpio_pinout.md`](docs/gpio_pinout.md); Knob 1's pins
  moved (DT 33 -> 25, SW 25 -> 39, green LED 14 -> 23); knobs 2-4 and the hub LEDs were re-pinned so every knob line is an RTC wake pin, so rewire before
  flashing.
- The full 4-hub build (`train-controller.yaml`) hasn't been run on real
  hardware with all hubs present. It now has the same controls as the dev
  config (knobs, buttons, LEDs, sleep) via `controls.yaml`.
- **LEGO Remote support is compile-tested only.** The remote button byte
  values, battery property and LED mode come from the public LWP3 docs and
  still need confirming on hardware; set `remote1_mac`/`remote2_mac` in
  `esphome/secrets.yaml` (placeholders until then). 6 BLE connection slots
  (5 hubs; remotes currently disabled in the dev config) plus Wi-Fi on
  a classic ESP32 is untested.

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
- LEGO Powered Up Remote (Handset 88010) support, via `remote-package.yaml`:
  2 remotes are registered, one per control slot
  (slot 1 drives hub1+hub2, slot 2 drives hub3+hub4; left button group ->
  first hub, right -> second; `+`/`-` step speed by 10, red stops), with button `binary_sensor`s,
  a battery `sensor` and an RGB LED `light`. Hubs stop if their remote
  disconnects.
- Per-hub BLE enable/disable switch (park a hub without power-cycling it).
- A local web UI (`http://<device-ip>/`) with no Home Assistant dependency.
- Battery operation (4×AA, no physical power switch): deep sleep after an
  inactivity timeout (2 min when every motor is stopped, 10 min otherwise, with
  a 30 s blinking warning; all motors are zeroed first), woken back up by
  turning or pressing any knob (EXT1 `ANY_HIGH` on all 12 knob lines), with a
  status LED per hub plus a dual-color (common-anode) LED: the second channel
  is blue on the current board - blue = a hub is connected, purple (red+blue) =
  none, alternating with red = battery low (measured through a divider on
  GPIO36).
- Hub 5 is an alternative to Hub 1 (only one of the two connects; they share
  Knob 1 and its status LED). Remotes are disabled in the dev config for now.
- Zero detent: each knob has a +-2 tick dead zone around speed 0.
- Port-B headlight: a port B number with `light: true` drives a Powered Up
  light, on while the hub moves; hold the knob button and turn to dim it by
  hand (a plain click returns it to automatic). No hub enables it yet.
- Two extra buttons (GPIO18/19, see `docs/gpio_pinout.md`): **Couple** toggles
  coupled mode (syncs all connected hubs to the last knob speed, then any knob
  drives all of them; the connected hubs' LEDs blink in unison while coupled) and **Stop All** zeroes every hub. They do not wake the
  board from deep sleep.

## Hardware

- Generic ESP32 DevKitC (`esp32dev`, esp-idf framework).
- LEGO Powered Up hubs — currently wired: Hub 1 (Duplo Train Base), Hub 2 &
  Hub 3 (plain Powered Up Train Hub). A 4th hub slot exists in the full
  config, pending its MAC.
- Four KY-040 rotary encoders with integrated pushbuttons (one per hub), wired active-high (module `+` to GND, GND to 3.3V).
- 4 LEDs (one per connected hub + one "awake" indicator).
- 4×AA batteries into the board's 5V/VIN pin.

See [`docs/gpio_pinout.md`](docs/gpio_pinout.md) for the full pin map.

## Repo layout

```
esphome/
  common/              shared base.yaml (wifi/api/ota/web_server), ble_base.yaml, safety.yaml
  hub-package.yaml      reusable per-hub block for the full config
  controls.yaml         shared physical controls (knobs, buttons, LEDs, battery, sleep) for both configs
  train-controller.yaml        the full build (5 hubs via hub-package.yaml, same controls, remotes disabled)
  train-controller-dev.yaml    single-board bring-up config (5 hubs - Hub 1/Hub 5 are mutually exclusive alternatives on Knob 1 - remotes disabled, 4 knobs, LEDs, deep sleep - the active dev/test config)
  knob-package.yaml     reusable per-knob block (encoder + button), used by controls.yaml
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

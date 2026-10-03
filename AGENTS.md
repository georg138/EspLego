# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An ESPHome device (ESP32) that drives multiple LEGO Powered Up train hubs
over Bluetooth LE, implementing the LEGO Wireless Protocol v3 (LWP3) as a
custom ESPHome `external_component` (`external_components/lwp3_hub/`) on top
of ESPHome's native `ble_client`. See `PLAN.md` for the full architecture
rationale (why a custom component instead of porting an existing Arduino
library) and build history; `README.md` for current feature/status summary.

## Commands

All commands run from the repo root unless noted. Requires the project venv:

```sh
source .venv/bin/activate
```

If this is a fresh checkout, create it first: `python3 -m venv .venv && source .venv/bin/activate && pip install esphome`.

**Validate a config** (fast, no compile):
```sh
esphome config esphome/train-controller-dev.yaml
```

**Compile** (the real test — there is no other test suite in this repo; a
clean compile against the real ESP-IDF toolchain is how every change here
gets verified):
```sh
esphome compile esphome/train-controller-dev.yaml
```

**Flash + watch logs** (requires real hardware connected):
```sh
esphome run esphome/train-controller-dev.yaml
```

Two top-level configs:
- `esphome/train-controller-dev.yaml` — the active single-board dev/bring-up
  config (currently 3 hubs + rotary encoder knob + status LEDs + deep sleep).
  This is the one that gets changed and tested first.
- `esphome/train-controller.yaml` — the full 4-hub build, using
  `esphome/hub-package.yaml` as a reusable per-hub template via
  `packages:`/`vars:`. Features get backported here once confirmed in the
  dev config (see "Dev config vs full config" below).

**SSL cert errors during compile** (`CERTIFICATE_VERIFY_FAILED` downloading
PlatformIO packages): set `SSL_CERT_FILE`/`REQUESTS_CA_BUNDLE` to a cert
bundle containing public CA roots before compiling:
```sh
export SSL_CERT_FILE=$(python3 -m certifi)
export REQUESTS_CA_BUNDLE=$SSL_CERT_FILE
```

**Build cache**: `esphome/.esphome/` holds CMake/Ninja's incremental build
state. Leave it in place between compiles — deleting it forces a full
ESP-IDF framework rebuild (1000+ steps) instead of an incremental one. It's
already gitignored.

**Secrets**: `esphome/secrets.yaml` (gitignored; copy from
`esphome/secrets.yaml.example`) must live next to the YAML configs — ESPHome
resolves `!secret` relative to the config file's directory, not the repo
root.

## Architecture

### `external_components/lwp3_hub/` — the custom component

- `lwp3_protocol.{h,cpp}`: pure LWP3 byte encode/decode (message framing,
  motor power, Duplo speaker sounds, Hub Attached I/O parsing). No
  ESPHome/BLE includes — written fresh against LEGO's public protocol docs,
  not ported from any reference implementation (licensing — see below).
- `lwp3_hub.{h,cpp}`: `LWP3Hub` class, one instance per physical hub,
  extends `ble_client::BLEClientNode` + `Component`. Lifecycle: connect
  (handled by ESPHome's `ble_client`) → discover the LWP3 GATT
  service/characteristic → subscribe to notify → parse Hub Attached I/O to
  learn live port topology → accept writes → encode Port Output Command.
  **Important**: the characteristic handle is cached once at
  `ESP_GATTC_SEARCH_CMPL_EVT` and reused for all later writes via
  `esp_ble_gattc_write_char()` directly — do not call `get_characteristic()`
  again later, since ESPHome releases its GATT cache once every registered
  `BLEClientNode` reaches `ESTABLISHED`, after which it returns `nullptr`.
- `number/` and `button/` subdirectories: ESPHome platform implementations
  (`LWP3MotorNumber`, `LWP3SoundButton`), each `Parented<LWP3Hub>`, calling
  back into the hub instance rather than touching BLE directly.
- `LWP3MotorNumber::control()` applies a direction-aware `min_power`
  deadband: increasing from 0 into the dead zone lifts to `min_power`
  (so the motor actually engages instead of humming), decreasing into it
  snaps to 0 (so you can actually stop/slow down, rather than getting stuck
  at `min_power`). This needs the previous raw value to detect direction —
  see `last_raw_value_` in `lwp3_motor_number.cpp`.

### YAML structure

- `esphome/common/base.yaml`: shared `esphome:`/`esp32:`/`wifi:`/`api:`/
  `ota:`/`web_server:` — included by every top-level config via `packages:`.
- `esphome/common/ble_base.yaml`: `esp32_ble_tracker:`/`esp32_ble:`. Scan
  parameters are deliberately low-duty-cycle passive scanning
  (`active: false`, `window: 30ms` of a `320ms` interval) — a defaulted
  window auto-raises to match the interval (100% duty continuous active
  scanning) once Wi-Fi is configured, which competes for radio time with
  maintaining multiple live `ble_client` connections and was an observed
  cause of flaky BLE drops.
- `esphome/common/safety.yaml`: the global "Stop All Trains" switch (full
  config only).
- `esphome/hub-package.yaml`: reusable per-hub block (`ble_client` +
  `lwp3_hub` + per-port `number`), parametrized via substitutions
  (`hub_id`, `hub_mac`, `hub_name`, `min_power_a`, `min_power_b`). Hub-specific
  extras that aren't generic across all hubs (e.g. one hub's Duplo speaker
  sound buttons) don't belong in this shared template — add them as a
  one-off block in the top-level config instead, targeting that hub's
  `lwp3_hub_id` directly.
- Physical GPIO wiring (rotary encoder, buttons, status LEDs, deep sleep
  wake pin) lives only in `train-controller-dev.yaml`, never in
  `hub-package.yaml` — it's board-specific, not something that can be
  parametrized per virtual hub. See `docs/gpio_pinout.md` for the current
  pin map and reserved pins for future knobs.

### Dev config vs full config

`train-controller-dev.yaml` is where new hub hardware gets brought up and
tested first (single board, fast iteration). Once a hub's quirks are
confirmed (which port is the motor, whether it needs `min_power`, whether it
has extra hardware like a speaker), that gets merged into
`train-controller.yaml`/`hub-package.yaml` via the `min_power_a`/
`min_power_b` substitution overrides and any one-off blocks — see how Hub 1
(Duplo Train Base, `min_power_a: "50"`, 5 sound buttons) differs from Hubs
2-4 (plain Powered Up Train Hub, `min_power_a: "10"` default) in
`train-controller.yaml`.

### Licensing

The combined `LICENSE` mirrors ESPHome's own split: `.c`/`.cpp`/`.h`/`.hpp`/
`.tcc`/`.ino` files under `external_components/` are GPLv3 (they derive from
ESPHome's own GPLv3-licensed C++ runtime classes — `Component`,
`ble_client::BLEClientNode`, `number::Number`, `button::Button`); everything
else (YAML, Python codegen/schema files, docs) is MIT. Don't port code from
`esp32_PoweredUp`, `legoino`, or similar reference implementations — they're
either unlicensed or the point of this project is a clean-room implementation
against LEGO's publicly-documented protocol.

### ESP32 hardware quirks worth knowing

- Reusing one GPIO across two components (e.g. a button used both as a
  `binary_sensor` and a `deep_sleep` wake pin) requires `allow_other_uses:
  true` on **every** config that references that pin, or ESPHome's pin
  validator rejects it with "Pin N is used in multiple places". (Not
  `ignore_pin_validation_error` — that flag is for a different, unrelated
  validation check.)
- Classic ESP32 (`esp32dev`) only supports `ANY_HIGH` or `ALL_LOW` for
  multi-pin deep-sleep wake (`esp32_ext1_wakeup`) — `ANY_LOW` is
  newer-chip-only. An active-low (pull-up, switch-to-GND) button/encoder
  wiring can't use `ANY_HIGH` cleanly without rewiring to active-high
  (pull-down, switch-to-3.3V) — which in turn can defeat a KY-040 module's
  onboard debounce filtering if its `+`/VCC pin is left disconnected to
  avoid the module's own pull-ups fighting the new pull-down. This project
  tried that rewire for knob-rotation-wake and reverted it after noisy
  readings; the knob stays active-low, only the dedicated button wakes the
  device.
- GPIO34-39 are input-only (no internal pull resistor) — fine for pure
  inputs (encoder/button lines) if the external module supplies its own
  pull-up/down, not usable for outputs (LEDs).

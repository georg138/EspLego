# GPIO pin plan (train-controller-dev.yaml)

This is the single source of truth for which GPIO is used for what on the dev
board, so future additions (more knobs, more LEDs) don't collide with
anything already wired. Update this file whenever a pin is assigned.

Board: generic ESP32 DevKitC (`esp32dev`). Avoid strapping pins (0, 2, 5, 12,
15), flash/PSRAM pins (6-11), and UART0 console pins (1, 3) for anything new.

## Currently wired

| Pin | Function | Notes |
|---|---|---|
| GPIO32 | Knob 1 - CLK | Active-low, module's onboard pull-up (module `+` to 3.3V, common to GND) |
| GPIO33 | Knob 1 - DT | Same as CLK |
| GPIO25 | Knob 1 - SW (button) | Active-low, internal pull-up + `inverted: true`. Also the deep-sleep wake pin (`wakeup_pin`, EXT0) |
| GPIO26 | Hub 1 status LED | On when Hub 1's BLE is connected |
| GPIO27 | Hub 2 status LED | On when Hub 2's BLE is connected |
| GPIO13 | Hub 3 status LED | On when Hub 3's BLE is connected |
| GPIO14 | Awake/asleep LED | On while awake, off right before deep sleep |

Design notes learned the hard way (see git history / conversation record if
needed):
- Knob 1 was briefly rewired active-high (common to 3.3V, internal
  pull-down) so rotation could also wake the device via `esp32_ext1_wakeup`
  (`ANY_HIGH`, multi-pin). That removed the KY-040 module's onboard debounce
  filtering (which assumes active-low/GND-referenced operation) and produced
  noisy, jumpy speed readings. **Reverted** - knob wiring stays active-low,
  only the button wakes the device (EXT0, single-pin `wakeup_pin`).
- Don't reuse this lesson's mistake for future knobs: wire them active-low
  (module `+` to 3.3V, common to GND) like Knob 1's current, working
  configuration - not active-high.

## Reserved for future hubs/knobs (not yet wired or in YAML)

| Pin | Planned function |
|---|---|
| GPIO4 | Hub 4 status LED |
| GPIO16 | Knob 2 - CLK |
| GPIO17 | Knob 2 - DT |
| GPIO18 | Knob 2 - SW (button) |
| GPIO19 | Knob 3 - CLK |
| GPIO21 | Knob 3 - DT |
| GPIO22 | Knob 3 - SW (button) |
| GPIO23 | Knob 4 - CLK |
| GPIO34 | Knob 4 - DT (input-only pin - fine here since no internal pull is needed, the module supplies its own) |
| GPIO35 | Knob 4 - SW (input-only pin - same reasoning; if this knob's button should also wake the device, note EXT0/`wakeup_pin` only supports ONE pin on classic ESP32, so multiple wake-capable buttons would need `esp32_ext1_wakeup` with `mode: ALL_LOW`, which requires all listed pins low at once - doesn't fit multiple independent buttons. Simplest: keep wake on Knob 1's button only, new knobs' buttons just stop their own hub while awake) |

Each future knob follows the same pattern as Knob 1 in
`train-controller-dev.yaml`: a `sensor: platform: rotary_encoder` block with
plain `pin_a`/`pin_b` (no explicit mode - relies on the module's own
pull-ups, same as Knob 1's reverted config) targeting only that hub's two
`number` ports, plus a `binary_sensor: platform: gpio` for its button
(`mode: {input: true, pullup: true}`, `inverted: true`) zeroing only that
hub's ports - not every hub, unlike Knob 1's current "master throttle"
behavior.

Still free after this allocation: GPIO36, GPIO37, GPIO38, GPIO39 (input-only).

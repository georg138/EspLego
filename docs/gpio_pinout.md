# GPIO pin plan (train-controller-dev.yaml)

This is the single source of truth for which GPIO is used for what on the dev
board, so future additions don't collide with anything already wired. Update
this file whenever a pin is assigned.

Board: Wemos-style **D1 mini ESP32** (ESP32-WROOM-32 module, classic ESP32, no
PSRAM). It breaks out fewer pins than a DevKitC - only the pins below exist on
the headers. Avoid strapping pins (0, 2, 5, 12, 15) for new outputs, flash pins
(6-11), and UART0 console pins (1, 3).

![Pin connection diagram](pinout.svg)

## Pin map by physical header column (top to bottom, as listed on the board)

| Header column | Pin | Function |
|---|---|---|
| Right outer | GPIO39 (SVN) | **Knob 1 SW** (input-only, relies on the module's pull-up) |
| | GPIO35 | **Knob 2 SW** (input-only, module pull-up) |
| | GPIO33 | **Knob 3 SW** |
| | GPIO34 | **Knob 4 SW** (input-only, module pull-up) |
| | GPIO14 (TMS) | **Wake line W** - EXT0 `wakeup_pin`, internal pull-up |
| Left outer | GPIO27 | Hub 2 status LED |
| | GPIO25 | Knob 1 DT |
| | GPIO32 | Knob 1 CLK |
| | GPIO4 | Hub 4 status LED |
| | GPIO2 | free (strapping pin, usable as input/output) |
| Left inner | GPIO22 | Knob 3 CLK |
| | GPIO21 | Knob 3 DT |
| | GPIO17 | Knob 2 CLK |
| | GPIO16 | Knob 2 DT |
| | GPIO15 (TDO) | free (strapping pin) |
| Right inner | GPIO36 (SVP) | Battery voltage ADC (100k/100k divider from VIN) |
| | GPIO26 | Hub 1 status LED |
| | GPIO18 | Knob 4 CLK |
| | GPIO19 | Knob 4 DT |
| | GPIO23 | Status LED - green |
| | GPIO5 | Status LED - red (strapping pin, output to an LED is fine) |
| | GPIO13 (TCK) | Hub 3 status LED |

Not usable: GPIO0 and GPIO12 (strapping - a button held at wake would break
boot), GPIO1/3 (UART0), SD1/SDD/SD2/SD3/CLK/CMD (flash). GPIO37/38 are not
broken out. Still free: **GPIO2, GPIO15**.

## Knobs (one per hub, see `esphome/knob-package.yaml`)

| Knob | Hub | CLK | DT | SW |
|---|---|---|---|---|
| 1 | hub1 | GPIO32 | GPIO25 | GPIO39 |
| 2 | hub2 | GPIO17 | GPIO16 | GPIO35 |
| 3 | hub3 | GPIO22 | GPIO21 | GPIO33 |
| 4 | hub4 | GPIO18 | GPIO19 | GPIO34 |

Wiring for every KY-040 is **active-low**: module `+` to 3.3V, GND to GND,
CLK/DT/SW to the pins. The module's own pull-ups hold all three lines high, so
no internal or external resistor is needed (and the input-only pins are fine).
Turning a knob steps its hub's current speed by 1 per detent; pressing it stops
that hub.

Pins that moved from the earlier single-knob wiring: Knob 1 DT 33 -> 25, Knob 1
SW 25 -> 39, green status LED 14 -> 23.

## Wake from deep sleep: any knob button (diode-OR, no resistors)

Classic ESP32 deep-sleep wake is EXT0 (one pin) or EXT1 (several, but only
`ANY_HIGH`/`ALL_LOW`). Rather than rewiring the buttons active-high, every knob
button is OR-ed onto one dedicated wake line with a 1N4148:

```
                       GPIO14 (W, internal pull-up)
                          |
        +-----------------+-----------------+-----------------+
        |                 |                 |                 |
      ->|-              ->|-              ->|-              ->|-     1N4148 x4
        |  anode on W     |                 |                 |      (bar/cathode toward the button)
   Knob1 SW (39)     Knob2 SW (35)     Knob3 SW (33)     Knob4 SW (34)
   (button to GND)   (button to GND)   (button to GND)   (button to GND)
```

- Anode on GPIO14, cathode on the knob's SW pad (the same net that goes to its
  own GPIO). Pressing a button pulls its SW net to GND, which pulls GPIO14 low
  through the diode; the other diodes stay reverse-biased (their SW nets are
  high), so buttons don't interact and each knob still reads only its own press.
- A 1N4148 carries only ~60 uA through the ~50k internal pull-up, so it drops
  ~0.4-0.5 V, well below the ESP32's low threshold (~0.8 V).
- GPIO14 is the EXT0 `wakeup_pin` (`inverted: true`, `pullup: true`); ESPHome
  enables the sleep pull-up and holds it through sleep. It is not shared with
  any button, so a Knob 2-4 press can't look like another knob's click while
  awake. Knob rotation does not wake the device.

## Design notes learned the hard way

- An earlier attempt rewired Knob 1 **active-high** (encoder common to 3.3V,
  internal pull-down) so rotation could wake the device via `esp32_ext1_wakeup`
  (`ANY_HIGH`). That removed the KY-040's onboard debounce for CLK/DT (it
  assumes active-low operation) and gave noisy, jumpy speed readings. Reverted.
  Don't wire knobs active-high.
- ESPHome's EXT1 path doesn't enable any internal pull in deep sleep (only the
  EXT0 `wakeup_pin` path does), which is another reason the diode-OR + EXT0
  approach is used.
- GPIO34-39 have no internal pulls and GPIO12 / GPIO0 must not be pulled the
  wrong way at boot, so those aren't used as the wake line.

## Other notes

- A fifth knob would need three more pins; only GPIO2 and GPIO15 (strapping)
  are free, so it would also need a hub/status LED moved or an I/O expander.
- The wake line's boot behaviour: GPIO14 emits a brief PWM at boot, harmless
  on an input behind reverse-biased diodes.

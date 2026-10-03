# EspLego — ESPHome Multi-Train LEGO Powered Up Controller

## Context

The goal: one ESP32 running ESPHome that remote-controls and automates **4+ LEGO Powered Up train hubs simultaneously**, with entities exposed to Home Assistant *and* some automation logic (like an emergency stop) that keeps working even if Home Assistant is down.

The user originally pointed at `github.com/lemio/esp32_PoweredUp` as the basis. Research established that repo is **not** an ESPHome component — it's a standalone Arduino/PlatformIO library that bundles its own NimBLE-Arduino BLE stack directly, has no YAML integration, and no LICENSE file. It demonstrates only 2 simultaneous BLE connections in its examples. No mature, licensed, ESPHome-native LEGO Powered Up integration exists anywhere (the closest reference, `corneliusmunz/legoino`, has the same "bundles its own BLE stack" problem and isn't licensed for reuse either).

ESPHome's own `esp32_ble_tracker`/`ble_client` components already support multiple simultaneous BLE peripheral connections from one ESP32 — this is the same pattern used for arrays of Xiaomi/inkbird BLE sensors. That means the right architecture is a **custom ESPHome `external_component`** built on ESPHome's native BLE client stack (not a port of esp32_PoweredUp/legoino code), implementing LEGO's openly-published Wireless Protocol v3 (LWP3) directly. This avoids running two independent BLE stacks in one firmware (which would conflict) and gives clean YAML config + automatic Home Assistant entity discovery via ESPHome's `api:`.

LWP3 essentials confirmed via LEGO's official docs (lego.github.io/lego-ble-wireless-protocol-docs):
- Service UUID `00001623-1212-efde-1623-785feabcd123`, single characteristic `00001624-1212-efde-1623-785feabcd123` (write-without-response for commands, notify for events).
- Message framing `[length][hub-id=0x00][message-type][payload...]`; key types: Hub Attached I/O (`0x04`, announces port topology), Port Output Command (`0x81`, motor/LED control), Port Value (`0x45`, incoming sensor/button notifications).

We write `lwp3_protocol.{h,cpp}` fresh against these public docs — using node-poweredup/pylgbst only as informal byte-layout cross-checks, never copying code — to avoid the licensing gap in the reference implementations.

The working directory (`/Users/georg/src/EspLego`) is currently empty and not yet a git repo; this becomes a new project from scratch.

## Architecture

**Custom component**: `external_components/lwp3_hub/`, a `LWP3Hub` class extending `esp32_ble_client::BLEClientNode` + `Component`. One instance per physical hub, bound 1:1 to its own `ble_client:` YAML block (by MAC address) — ESPHome's existing `ble_client` machinery already handles per-connection reconnect independently, so one hub dropping BLE never blocks the other three.

Lifecycle: connect (handled by `ble_client`) → discover LWP3 service/characteristic → subscribe to notify → parse Hub Attached I/O to learn live port topology (logged, compared against YAML-declared ports) → accept `number` writes → encode Port Output Command → parse incoming Port Value notifications (logged/dropped in v1, hook point for future sensors). On disconnect: reset internal state, zero published port states, and expect fresh Hub Attached I/O after reconnect (LEGO hubs re-announce every time).

**v1 entity surface (deliberately minimal — don't gold-plate)**:
- `number` per motor port (−100..100), the actual train control surface.
- Connect/disconnect handling (including zeroing motor state) lives inside the component itself, which hooks `ESP_GATTC_OPEN_EVT`/`ESP_GATTC_CLOSE_EVT` directly (confirmed via ESPHome's `ble_client`/`esp32_ble_client` source: `BLEClientNode::gattc_event_handler` receives every GATT event, not just notify/search — see `ble_client.cpp`'s broadcast loop). No separate `on_connect`/`on_disconnect` YAML automation or custom binary_sensor needed for this in v1.

**v2, explicitly deferred**: hub LED color (`light`), battery voltage (`sensor`), button press (`binary_sensor`), RSSI. Add only if/when requested — LWP3 message parsing for these adds real complexity that isn't needed to drive trains.

**Protocol code separation**: `lwp3_protocol.{h,cpp}` holds pure encode/decode functions with no ESPHome/BLE includes, independently testable, keeping `lwp3_hub.cpp` focused on connection lifecycle/state.

## Repo layout

```
EspLego/
├── .gitignore                    # secrets.yaml, .esphome/, *.bin, .pioenvs/
├── secrets.yaml.example          # committed template
├── secrets.yaml                  # gitignored: wifi creds, api key, ota password, 4 hub MACs
├── esphome/
│   ├── common/
│   │   ├── base.yaml             # esp32:, logger:, wifi:, api:, ota:, sdkconfig_options
│   │   ├── ble_base.yaml         # single esp32_ble_tracker: block (shared by all hubs)
│   │   └── safety.yaml           # global "Stop All Trains" switch
│   ├── hub-package.yaml          # reusable per-hub block: ble_client + lwp3_hub + number x N ports
│   └── train-controller.yaml     # top-level device: packages base/ble/safety + 4x hub-package via vars
├── external_components/lwp3_hub/
│   ├── __init__.py               # YAML schema + codegen (hub name, ble_client_id, ports map)
│   ├── lwp3_hub.h / .cpp         # LWP3Hub class (BLEClientNode lifecycle, state machine)
│   ├── lwp3_protocol.h / .cpp    # pure LWP3 byte encode/decode, no BLE deps
│   └── number/__init__.py, lwp3_motor_number.h/.cpp   # motor power `number` platform
├── docs/ble_mac_scan.md
└── tools/scan_hubs.yaml          # throwaway config: esp32_ble_tracker on_ble_advertise, logs nearby MACs
```

`external_components` is referenced locally (`type: local`), no separate repo needed for v1. `hub-package.yaml` is instantiated 4x from `train-controller.yaml` via ESPHome's `packages:` + `vars:` substitution — this is what avoids repeating near-identical YAML per hub (unique `id`, `ble_client_id`, and MAC per instantiation).

`esp32:` framework choice: **esp-idf** (not Arduino) — needed for simultaneous BLE connection tuning. Verified against current ESPHome source (`esphome/components/esp32_ble/__init__.py`): connection count is set via `esp32_ble: max_connections: N` (default 3, max 9 on ESP-IDF) — this YAML option handles the underlying `CONFIG_BT_ACL_CONNECTIONS`/`CONFIG_BTDM_CTRL_BLE_MAX_CONN` sdkconfig options automatically, no manual `sdkconfig_options:` needed. Each `ble_client:` block consumes exactly one connection slot (`esp32_ble.consume_connection_slots`), so 4 hubs needs `max_connections: 4`.

## On-device automation vs Home Assistant split

- **On-device** (survives HA/Wi-Fi outage): a global "Stop All Trains" template switch that zeroes every hub/port `number` entity; per-hub auto-zero-on-BLE-disconnect built into the component itself (belt-and-suspenders, works even without the switch).
- **Home Assistant**: everything else — dashboards (free via `api:` auto-discovery), schedules, cross-device orchestration, notifications. Don't duplicate this on-device; HA is easier to iterate on for anything beyond the safety floor.

## Phased build plan

1. **Phase 0 — Prep**: identify actual hub models (City/Train Hub = 2 ports A/B; Technic Hub = 4 ports A–D — affects `ports:` config, not the design). Flash `tools/scan_hubs.yaml` to find each hub's BLE MAC (LWP3 has no pair-by-button flow; `ble_client:` needs MACs up front). `git init`, commit skeleton.
2. **Phase 1 — Single hub PoC**: minimal `lwp3_hub` component (connect → discover → subscribe → parse Hub Attached I/O → one `number` on port A → send Port Output Command). Verify: compiles, connects (watch `esphome logs`), Hub Attached I/O matches physical ports, **physically confirm motor spins** via HA/Developer Tools slider, confirm direction sign and 0 = stop, confirm disconnect/reconnect recovers without reflash.
3. **Phase 2 — Generalize to 4 hubs**: refactor into `hub-package.yaml` + `packages:`, add `sdkconfig_options`. Verify: compiles with all 4, all 4 physically connect and report topology, exercise all 8 `number` entities with no cross-talk, power-cycle one hub mid-session and confirm the other 3 keep working uninterrupted (validates per-hub independence). If instability appears at 4 connections, first tune BLE scan params, then consider ESP32-S3 (user is open to this).
4. **Phase 3 — Polish**: add "Hub Connected" triggers, `safety.yaml` Stop All Trains switch, HA dashboard cards. Verify Stop All Trains actually halts all physical trains, and works with Wi-Fi disabled (simulated HA outage).
5. **Phase 4 (only if requested)**: v2 entities — hub LED, battery, button.

## Risks / open questions

- Exact hub port count/model unconfirmed — plan defaults to 2 ports (A/B); trivial to extend to 4 if Technic hubs are involved.
- `sdkconfig_options` key names may shift across ESP-IDF versions — Phase 2 compile is the verification point.
- LWP3 motor writes are fire-and-forget (write-without-response, no ack); no retry logic in v1 — revisit only if missed commands are observed in practice.
- No code ported from esp32_PoweredUp or legoino (licensing gaps); implementation is clean-room against LEGO's public protocol docs, which is slower but avoids IP risk.

## Critical files to create first

- `external_components/lwp3_hub/lwp3_protocol.cpp` / `.h` — protocol encode/decode
- `external_components/lwp3_hub/lwp3_hub.cpp` / `.h` — BLEClientNode lifecycle
- `external_components/lwp3_hub/__init__.py` — YAML schema
- `external_components/lwp3_hub/number/__init__.py` — motor number platform
- `esphome/hub-package.yaml`, `esphome/train-controller.yaml`

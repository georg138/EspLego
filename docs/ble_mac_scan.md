# Finding each hub's BLE MAC address

`ble_client:` in ESPHome connects by MAC address — there's no pair-by-button
discovery flow, so each hub's MAC must be known ahead of time.

1. Fill in `wifi_ssid`, `wifi_password`, `ota_password` in `esphome/secrets.yaml`
   (copy from `esphome/secrets.yaml.example` if you haven't already).
2. Flash the scanner to a spare ESP32:
   ```sh
   esphome run esphome/scan_hubs.yaml
   ```
3. With the ESP32 running and logs streaming, power on ONE LEGO hub at a time
   (press its green button). Watch for a new `Found: AA:BB:CC:DD:EE:FF  RSSI: -xx`
   line appearing right after you press the button — that's very likely the hub
   you just turned on (LEGO hubs don't broadcast a friendly name in advertising
   data, so timing/RSSI is the easiest way to identify them).
4. Power off that hub, wait a few seconds for the log to settle, then power on
   the next hub and repeat.
5. Record all 4 MACs into `secrets.yaml` as `hub1_mac`, `hub2_mac`, `hub3_mac`,
   `hub4_mac` (and `hub5_mac` for the alternative to hub 1).

If you have LightBlue, nRF Connect, or a similar BLE scanner app, you can
cross-check by looking for devices advertising service UUID
`00001623-1212-efde-1623-785feabcd123` (the LEGO Wireless Protocol v3 service).

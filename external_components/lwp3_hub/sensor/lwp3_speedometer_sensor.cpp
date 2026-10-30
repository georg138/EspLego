#include "lwp3_speedometer_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace lwp3_hub {

static const char *const TAG = "lwp3_hub.speedometer";

void LWP3SpeedometerSensor::setup() {
  this->parent_->register_port_listener({
      lwp3_protocol::DUPLO_PORT_SPEEDOMETER,
      lwp3_protocol::MODE_SPEEDOMETER_SPEED,
      [this](const uint8_t *payload, size_t len) {
        // Payload is a signed 16-bit little-endian wheel speed (verified on a Duplo
        // base: stopped = 0). Raw bytes are logged at VERBOSE for calibration.
        ESP_LOGV(TAG, "raw payload (%u bytes): %s", static_cast<unsigned>(len), format_hex_pretty(payload, len).c_str());
        if (len == 1) {
          this->publish_state(static_cast<int8_t>(payload[0]));
        } else if (len >= 2) {
          this->publish_state(static_cast<int16_t>(payload[0] | (payload[1] << 8)));
        }
      },
      nullptr,
      [this]() { this->publish_state(NAN); },
  });
}

void LWP3SpeedometerSensor::dump_config() { LOG_SENSOR("", "LWP3 Speedometer", this); }

}  // namespace lwp3_hub
}  // namespace esphome

#endif

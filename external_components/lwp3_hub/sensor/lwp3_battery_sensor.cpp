#include "lwp3_battery_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace lwp3_hub {

static const char *const TAG = "lwp3_hub.battery";

void LWP3BatterySensor::setup() {
  this->parent_->register_battery_listener([this](uint8_t percent) { this->publish_state(percent); },
                                           [this]() { this->publish_state(NAN); });
}

void LWP3BatterySensor::dump_config() { LOG_SENSOR("", "LWP3 Battery", this); }

}  // namespace lwp3_hub
}  // namespace esphome

#endif

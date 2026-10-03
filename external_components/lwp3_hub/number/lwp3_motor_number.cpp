#include "lwp3_motor_number.h"
#include "esphome/core/log.h"
#include <cstdlib>

#ifdef USE_ESP32

namespace esphome {
namespace lwp3_hub {

static const char *const TAG = "lwp3_hub.number";

void LWP3MotorNumber::setup() {
  this->parent_->register_motor(this->port_, this);
  this->publish_state(0);
}

void LWP3MotorNumber::dump_config() { LOG_NUMBER("", "LWP3 Motor Power", this); }

void LWP3MotorNumber::control(float value) {
  const int32_t raw = static_cast<int32_t>(value);
  int32_t power = raw;
  const int32_t min_power = static_cast<int32_t>(this->min_power_);

  if (power != 0 && std::abs(power) < min_power) {
    const bool increasing = std::abs(raw) > std::abs(this->last_raw_value_);
    if (increasing) {
      // Heading up from 0 (or from another below-threshold value): lift to
      // min_power_ so the motor actually engages instead of just humming.
      power = (power > 0) ? min_power : -min_power;
    } else {
      // Heading down into the dead zone: snap to a real stop instead of
      // getting stuck at min_power_, unable to go any lower.
      power = 0;
    }
  }
  this->last_raw_value_ = raw;

  // LWP3 motor writes are write-without-response (no ack), so this is
  // published optimistically - using the remapped value so the UI reflects
  // what was actually sent.
  this->publish_state(power);
  this->parent_->set_motor_power(this->port_, static_cast<int8_t>(power));
}

}  // namespace lwp3_hub
}  // namespace esphome

#endif

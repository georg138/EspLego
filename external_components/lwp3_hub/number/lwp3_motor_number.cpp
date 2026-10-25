#include "lwp3_motor_number.h"
#include "esphome/core/log.h"
#include <algorithm>
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

void LWP3MotorNumber::apply_light_() {
  const int32_t power = this->light_override_ >= 0 ? this->light_override_ : (this->last_raw_value_ != 0 ? 100 : 0);
  this->publish_state(power);
  this->parent_->set_motor_power(this->port_, static_cast<int8_t>(power));
}

void LWP3MotorNumber::set_light_override(int override_value) {
  if (!this->light_)
    return;
  this->light_override_ = override_value < 0 ? -1 : std::min(100, override_value);
  this->apply_light_();
}

void LWP3MotorNumber::reset() {
  this->light_override_ = -1;
  this->last_raw_value_ = 0;
  this->publish_state(0);
}

void LWP3MotorNumber::control(float value) {
  const int32_t raw = static_cast<int32_t>(value);
  if (this->light_) {
    // A light has no direction or deadband: remember only whether it was asked
    // to move; apply_light_() turns that into on/off unless overridden.
    this->last_raw_value_ = raw;
    this->apply_light_();
    return;
  }
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
  // Remember what was actually applied, not the raw request: after a snap to 0
  // the next step up (raw 1) must compare against 0 to count as "increasing",
  // not against the stale below-threshold raw (e.g. 9), or it re-snaps to 0.
  this->last_raw_value_ = power;

  // LWP3 motor writes are write-without-response (no ack), so this is
  // published optimistically - using the remapped value so the UI reflects
  // what was actually sent.
  this->publish_state(power);
  this->parent_->set_motor_power(this->port_, static_cast<int8_t>(power));
}

}  // namespace lwp3_hub
}  // namespace esphome

#endif

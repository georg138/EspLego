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
  // Stateless mapping: 0 stays 0, any nonzero request 1..100 maps linearly onto
  // min_power_..100, so a motor that needs a minimum power never sits in a dead
  // zone and no direction tracking (lift/snap) is needed. Sign is preserved.
  const int32_t min_power = static_cast<int32_t>(this->min_power_);
  const int32_t mag = std::min<int32_t>(100, std::abs(raw));
  int32_t power = 0;
  if (mag != 0)
    power = min_power + ((mag - 1) * (100 - min_power) + 49) / 99;
  if (raw < 0)
    power = -power;

  // LWP3 motor writes are write-without-response (no ack), so this is
  // published optimistically. The published value is the REQUESTED speed (what
  // the UI, knobs and remotes read back and step from), not the mapped power.
  this->publish_state(raw);
  this->parent_->set_motor_power(this->port_, static_cast<int8_t>(power));
}

}  // namespace lwp3_hub
}  // namespace esphome

#endif

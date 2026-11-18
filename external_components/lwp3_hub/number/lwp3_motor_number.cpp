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

void LWP3MotorNumber::set_power_limits(int min_power, int max_power) {
  this->min_power_ = static_cast<uint8_t>(std::max(0, std::min(100, min_power)));
  this->max_power_ = static_cast<uint8_t>(std::max(0, std::min(100, max_power)));
  ESP_LOGI(TAG, "Port %u power %u..%u%%", this->port_, this->min_power_, this->max_power_);
  this->refresh();
}

void LWP3MotorNumber::reset() {
  this->light_override_ = -1;
  this->last_raw_value_ = 0;
  this->last_power_ = 0;
  this->publish_state(0);
}

void LWP3MotorNumber::brake() {
  if (this->light_) {
    this->control(0);
    return;
  }
  this->last_raw_value_ = 0;  // so refresh() doesn't restart it
  this->publish_state(0);
  this->send_power_(lwp3_protocol::MOTOR_POWER_BRAKE);
}

void LWP3MotorNumber::refresh() {
  if (this->light_ || this->last_raw_value_ == 0)
    return;
  const int8_t power = this->mapped_power_(this->last_raw_value_);
  if (power != this->last_power_)
    this->send_power_(power);
}

int8_t LWP3MotorNumber::mapped_power_(int32_t raw) const {
  // Stateless mapping: 0 stays 0, any nonzero request 1..100 maps linearly onto
  // min_power_..max_power_, so a motor that needs a minimum power never sits in
  // a dead zone and no direction tracking (lift/snap) is needed. Sign is
  // preserved. Both ends are % of the full-battery voltage, turned into PWM for
  // the current voltage by LWP3Hub::scaled_power().
  const int32_t min_power = this->parent_->scaled_power(this->min_power_);
  const int32_t cap = std::max(min_power, this->parent_->scaled_power(this->max_power_));
  const int32_t mag = std::min<int32_t>(100, std::abs(raw));
  int32_t power = 0;
  if (mag != 0)
    power = min_power + ((mag - 1) * (cap - min_power) + 49) / 99;
  return static_cast<int8_t>(raw < 0 ? -power : power);
}

void LWP3MotorNumber::send_power_(int8_t power) {
  this->last_power_ = power;
  this->parent_->set_motor_power(this->port_, power);
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
  this->last_raw_value_ = raw;
  // Published optimistically (the hub only confirms later, via command
  // feedback, see LWP3Hub::loop()). The published value is the REQUESTED speed (what
  // the UI, knobs and remotes read back and step from), not the mapped power.
  this->publish_state(raw);
  this->send_power_(this->mapped_power_(raw));
}

}  // namespace lwp3_hub
}  // namespace esphome

#endif

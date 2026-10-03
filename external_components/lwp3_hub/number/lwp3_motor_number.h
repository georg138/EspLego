#pragma once

#ifdef USE_ESP32

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../lwp3_hub.h"

namespace esphome {
namespace lwp3_hub {

class LWP3MotorNumber : public number::Number, public Component, public Parented<LWP3Hub> {
 public:
  void setup() override;
  void dump_config() override;

  void set_port(uint8_t port) { this->port_ = port; }
  /// Some motors (e.g. the Duplo Train Base's) need a minimum power before
  /// they overcome static friction and actually move - a nonzero value below
  /// this threshold would just hum in place. 0 (default) disables remapping.
  void set_min_power(uint8_t min_power) { this->min_power_ = min_power; }

 protected:
  void control(float value) override;

  uint8_t port_{0};
  uint8_t min_power_{0};
  // Last raw (pre-remap) requested value, used to tell whether a new request
  // below min_power_ is heading up from 0 (lift to min_power_, so the motor
  // still engages) or down from above (snap to 0, so it can actually stop/
  // slow down instead of getting stuck at min_power_).
  int32_t last_raw_value_{0};
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

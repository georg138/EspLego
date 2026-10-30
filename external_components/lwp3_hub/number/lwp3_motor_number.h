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
  /// they overcome static friction and actually move. Requests 1..100 are
  /// mapped linearly onto min_power..100 (0 stays 0). 0 (default) = no remap.
  void set_min_power(uint8_t min_power) { this->min_power_ = min_power; }
  /// This port drives a Powered Up light, not a motor (see control()).
  void set_light(bool light) { this->light_ = light; }
  bool is_light() const { return this->light_; }
  /// Light ports only: -1 = automatic (on while the last request was nonzero),
  /// 0..100 = manual brightness that survives motion requests. No-op on a
  /// motor port. Applies immediately.
  void set_light_override(int override_value);
  int light_override() const { return this->light_override_; }
  /// Called by the hub on disconnect: back to automatic and published as 0.
  void reset();

 protected:
  void control(float value) override;
  void apply_light_();

  uint8_t port_{0};
  uint8_t min_power_{0};
  bool light_{false};
  int light_override_{-1};
  // Last requested value; light ports only use it to know whether the hub was
  // asked to move (automatic headlight).
  int32_t last_raw_value_{0};
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

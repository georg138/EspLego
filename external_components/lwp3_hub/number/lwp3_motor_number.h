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
  void set_min_power(uint8_t min_power) { this->min_power_ = this->min_power_default_ = min_power; }
  /// Request 100 maps to this share (%) of the hub's full-battery voltage (see
  /// LWP3Hub::scaled_power()), so 1..100 maps onto min_power..that power
  /// (min_power is a share of the full-battery voltage too).
  void set_max_power(uint8_t max_power) { this->max_power_ = this->max_power_default_ = max_power; }
  /// Change min_power / max_power at runtime (clamped to 0..100, a running
  /// motor follows at once); reset_power_limits() puts the configured values
  /// back (the hub calls it on every connect).
  void set_power_limits(int min_power, int max_power);
  void reset_power_limits() { this->set_power_limits(this->min_power_default_, this->max_power_default_); }
  uint8_t min_power() const { return this->min_power_; }
  uint8_t max_power() const { return this->max_power_; }
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
  /// Stops hard: a motor port brakes (instead of coasting like a 0 request),
  /// a light port behaves like a 0 request. Published as 0.
  void brake();
  /// Called by the hub when its battery voltage changed: re-sends a running
  /// motor's power if the new cap changes it.
  void refresh();

 protected:
  void control(float value) override;
  void apply_light_();
  int8_t mapped_power_(int32_t raw) const;
  void send_power_(int8_t power);

  uint8_t port_{0};
  uint8_t min_power_{0};
  uint8_t max_power_{100};
  uint8_t min_power_default_{0};  // configured min_power
  uint8_t max_power_default_{100};  // configured max_power
  int8_t last_power_{0};  // last power sent to a motor port
  bool light_{false};
  int light_override_{-1};
  // Last requested value: a light port uses it to know whether the hub was
  // asked to move (automatic headlight), a motor port to re-map it in refresh().
  int32_t last_raw_value_{0};
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

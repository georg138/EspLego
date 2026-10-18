#pragma once

#ifdef USE_ESP32

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../lwp3_hub.h"

namespace esphome {
namespace lwp3_hub {

/// The Remote's RGB status LED (fixed port, REMOTE_PORT_LED).
class LWP3RemoteLed : public light::LightOutput, public Component, public Parented<LWP3Hub> {
 public:
  void setup() override;
  void dump_config() override;

  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  // Last requested color, re-sent whenever the LED port (re)attaches since
  // the remote forgets it across a disconnect.
  uint8_t r_{0}, g_{0}, b_{0};
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

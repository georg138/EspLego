#pragma once

#ifdef USE_ESP32

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../lwp3_hub.h"

namespace esphome {
namespace lwp3_hub {

/// One button (+ / red / -) of one Remote button group. Several of these
/// share a group's port; each reports pressed while the group's single-byte
/// value equals its own button code.
class LWP3RemoteButton : public binary_sensor::BinarySensor, public Component, public Parented<LWP3Hub> {
 public:
  void setup() override;
  void dump_config() override;

  void set_port(uint8_t port) { this->port_ = port; }
  void set_button_value(uint8_t value) { this->button_value_ = value; }

 protected:
  uint8_t port_{0};
  uint8_t button_value_{0};
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

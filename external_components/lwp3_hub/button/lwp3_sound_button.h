#pragma once

#ifdef USE_ESP32

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../lwp3_hub.h"
#include "../lwp3_protocol.h"

namespace esphome {
namespace lwp3_hub {

class LWP3SoundButton : public button::Button, public Component, public Parented<LWP3Hub> {
 public:
  void dump_config() override;

  void set_port(uint8_t port) { this->port_ = port; }
  void set_sound(lwp3_protocol::DuploSound sound) { this->sound_ = sound; }

 protected:
  void press_action() override;

  uint8_t port_{0};
  lwp3_protocol::DuploSound sound_{lwp3_protocol::DuploSound::HORN};
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

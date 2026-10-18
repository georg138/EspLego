#include "lwp3_remote_button.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace lwp3_hub {

static const char *const TAG = "lwp3_hub.remote_button";

void LWP3RemoteButton::setup() {
  this->publish_initial_state(false);
  this->parent_->register_port_listener({
      this->port_,
      lwp3_protocol::MODE_REMOTE_BUTTONS,
      [this](const uint8_t *payload, size_t len) {
        if (len >= 1)
          this->publish_state(payload[0] == this->button_value_);
      },
      nullptr,
      [this]() { this->publish_state(false); },
  });
}

void LWP3RemoteButton::dump_config() {
  LOG_BINARY_SENSOR("", "LWP3 Remote Button", this);
  ESP_LOGCONFIG(TAG, "  Port: %u, button value: 0x%02X", this->port_, this->button_value_);
}

}  // namespace lwp3_hub
}  // namespace esphome

#endif

#include "lwp3_remote_led.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace lwp3_hub {

static const char *const TAG = "lwp3_hub.remote_led";

void LWP3RemoteLed::setup() {
  this->parent_->register_port_listener({
      lwp3_protocol::REMOTE_PORT_LED,
      lwp3_protocol::MODE_LED_RGB,
      nullptr,
      [this]() { this->parent_->set_led_rgb(lwp3_protocol::REMOTE_PORT_LED, this->r_, this->g_, this->b_); },
      nullptr,
  });
}

void LWP3RemoteLed::dump_config() { ESP_LOGCONFIG(TAG, "LWP3 Remote LED"); }

light::LightTraits LWP3RemoteLed::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void LWP3RemoteLed::write_state(light::LightState *state) {
  float red, green, blue;
  state->current_values_as_rgb(&red, &green, &blue);
  this->r_ = static_cast<uint8_t>(red * 255.0f + 0.5f);
  this->g_ = static_cast<uint8_t>(green * 255.0f + 0.5f);
  this->b_ = static_cast<uint8_t>(blue * 255.0f + 0.5f);
  this->parent_->set_led_rgb(lwp3_protocol::REMOTE_PORT_LED, this->r_, this->g_, this->b_);
}

}  // namespace lwp3_hub
}  // namespace esphome

#endif

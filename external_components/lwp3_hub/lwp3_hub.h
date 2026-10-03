#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "lwp3_protocol.h"

#include <array>
#include <string>

namespace esphome {
namespace lwp3_hub {

// Powered Up hubs expose ports A-D (only A/B are wired on City/Train hubs;
// Technic hubs use all four).
constexpr uint8_t LWP3_MAX_PORTS = 4;

class LWP3MotorNumber;  // defined in number/lwp3_motor_number.h

class LWP3Hub : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override {}
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_hub_name(const std::string &name) { this->hub_name_ = name; }
  void add_declared_port(uint8_t port) { this->declared_ports_[port] = true; }

  /// Called from the corresponding `number` platform entity's setup() so this
  /// hub can zero its published state on disconnect.
  void register_motor(uint8_t port, LWP3MotorNumber *motor) { this->motors_[port] = motor; }

  /// Called by a child `number` entity's control(). No-ops (with a log
  /// warning) if the hub isn't connected/ready, or nothing is attached to
  /// this port yet.
  void set_motor_power(uint8_t port, int8_t power);

  /// Called by a child sound `button` entity's press_action(). Same no-op
  /// guards as set_motor_power().
  void play_sound(uint8_t port, lwp3_protocol::DuploSound sound);

 protected:
  void handle_search_complete_();
  void handle_notify_(const uint8_t *data, uint16_t len);
  void handle_disconnected_();
  /// Shared send path for both set_motor_power() and play_sound(): checks
  /// readiness/attachment, then writes `payload` write-without-response.
  /// Taken by value (not const&): esp_ble_gattc_write_char() needs a
  /// non-const uint8_t* into the payload bytes.
  void write_command_(uint8_t port, std::vector<uint8_t> payload);

  std::string hub_name_{"LWP3 Hub"};
  uint16_t char_handle_{0};
  bool ready_{false};

  std::array<bool, LWP3_MAX_PORTS> declared_ports_{};
  std::array<bool, LWP3_MAX_PORTS> attached_ports_{};
  std::array<LWP3MotorNumber *, LWP3_MAX_PORTS> motors_{};
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

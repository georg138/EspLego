#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "lwp3_protocol.h"

#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace esphome {
namespace lwp3_hub {

// Powered Up hubs expose ports A-D (only A/B are wired on City/Train hubs;
// Technic hubs use all four).
constexpr uint8_t LWP3_MAX_PORTS = 4;
// Highest port ID + 1 tracked for attach state. Covers the Remote's fixed
// ports (0, 1, 52) and the internal ports on other hubs (up to ~60).
constexpr uint8_t LWP3_MAX_PORT_IDS = 64;
// A motor/light command the hub hasn't confirmed (Port Output Command Feedback)
// within this time is re-sent, at most LWP3_COMMAND_MAX_RETRIES times.
constexpr uint32_t LWP3_COMMAND_RETRY_MS = 300;
constexpr uint8_t LWP3_COMMAND_MAX_RETRIES = 3;

class LWP3MotorNumber;  // defined in number/lwp3_motor_number.h

class LWP3Hub : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override {}
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_hub_name(const std::string &name) { this->hub_name_ = name; }
  void add_declared_port(uint8_t port) { this->declared_ports_[port] = true; }
  /// Full-battery voltage. Set: the hub's voltage sensor is read and
  /// scaled_power() holds motors at a fixed share of this voltage as it drains.
  void set_battery_full_mv(uint16_t mv) { this->battery_full_mv_ = mv; }

  /// Motor power (PWM %) that gives `pct` % of the full-battery voltage at the
  /// current battery voltage (up to 100). `pct` itself while the voltage is
  /// unknown or no full voltage is configured.
  int32_t scaled_power(uint8_t pct) const;

  /// Called from the corresponding `number` platform entity's setup() so this
  /// hub can zero its published state on disconnect.
  void register_motor(uint8_t port, LWP3MotorNumber *motor) { this->motors_[port] = motor; }

  /// Called by a child `number` entity's control(). No-ops (with a log
  /// warning) if the hub isn't connected/ready, or nothing is attached to
  /// this port yet. `power` may be lwp3_protocol::MOTOR_POWER_BRAKE. The command
  /// is re-sent until the hub confirms it (see LWP3_COMMAND_RETRY_MS).
  void set_motor_power(uint8_t port, int8_t power);

  /// Called from a sensor/binary_sensor/light entity's setup() to receive
  /// events for a port. `mode` is activated via Port Input Format Setup when
  /// the device attaches to that port (notifications on only if `on_value`
  /// is set). `on_attach` runs after that (e.g. to re-send a stored LED
  /// color); `on_reset` runs on detach/disconnect (publish unknown state).
  struct PortListener {
    uint8_t port;
    uint8_t mode;
    std::function<void(const uint8_t *payload, size_t len)> on_value;
    std::function<void()> on_attach;
    std::function<void()> on_reset;
  };
  void register_port_listener(PortListener listener) { this->port_listeners_.push_back(std::move(listener)); }

  /// Receives the hub battery percentage (Hub Properties update). The first
  /// registration makes the hub enable battery updates once connected.
  void register_battery_listener(std::function<void(uint8_t percent)> on_value, std::function<void()> on_reset) {
    this->battery_listeners_.push_back({std::move(on_value), std::move(on_reset)});
  }

  /// Sets an RGB LED port (e.g. the Remote's LED) to a color. Same no-op
  /// guards as set_motor_power().
  void set_led_rgb(uint8_t port, uint8_t r, uint8_t g, uint8_t b);

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
  /// Returns false if the checks dropped the command (nothing was sent).
  bool write_command_(uint8_t port, std::vector<uint8_t> payload);
  /// (Re-)sends pending_[port] and counts it as awaiting feedback.
  void send_pending_(uint8_t port);
  void handle_output_feedback_(const uint8_t *data, uint16_t len);
  void handle_port_info_(const uint8_t *data, uint16_t len);
  void handle_voltage_(const uint8_t *payload, size_t len);
  /// Writes without any port/attachment checks (still requires ready_).
  void write_raw_(std::vector<uint8_t> payload);
  bool is_attached_(uint8_t port) const { return port < LWP3_MAX_PORT_IDS && ((this->attached_ports_ >> port) & 1); }
  bool has_listener_(uint8_t port) const;
  static std::string port_label_(uint8_t port);

  std::string hub_name_{"LWP3 Hub"};
  uint16_t char_handle_{0};
  bool ready_{false};

  std::array<bool, LWP3_MAX_PORTS> declared_ports_{};
  uint64_t attached_ports_{0};  // bit N set = device attached on port N
  std::array<LWP3MotorNumber *, LWP3_MAX_PORTS> motors_{};

  // Latest motor/light command per port, kept until the hub confirms it.
  // `unacked` counts sends without a completed/discarded feedback yet; the hub
  // answers each command once, so 0 means the latest one arrived.
  struct PendingCommand {
    std::vector<uint8_t> payload;  // empty = nothing pending
    uint32_t sent_ms{0};
    uint8_t unacked{0};
    uint8_t retries{0};
  };
  std::array<PendingCommand, LWP3_MAX_PORTS> pending_{};
  // Retries only start once this hub has sent any feedback, so a hub that never
  // does is not flooded with re-sends.
  bool feedback_seen_{false};
  // Mode bitmasks from Port Information, to label the mode names when they arrive.
  std::array<uint16_t, LWP3_MAX_PORTS> input_modes_{};
  std::array<uint16_t, LWP3_MAX_PORTS> output_modes_{};

  // Battery voltage sensor (only used with battery_full_mv_ set). The raw value
  // is scaled with the mode-0 RAW/SI maxima the hub reports.
  uint16_t battery_full_mv_{0};
  uint8_t voltage_port_{0xFF};  // 0xFF = not attached
  float voltage_raw_max_{0};
  float voltage_si_max_{0};
  float voltage_mv_{NAN};  // smoothed
  float cap_mv_{NAN};      // voltage scaled_power() uses, only moved by >= 50 mV steps

  struct BatteryListener {
    std::function<void(uint8_t)> on_value;
    std::function<void()> on_reset;
  };
  std::vector<PortListener> port_listeners_;
  std::vector<BatteryListener> battery_listeners_;
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

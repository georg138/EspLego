#include "lwp3_hub.h"
#include "number/lwp3_motor_number.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace lwp3_hub {

namespace espbt = esphome::esp32_ble_tracker;

static const char *const TAG = "lwp3_hub";

void LWP3Hub::dump_config() {
  ESP_LOGCONFIG(TAG, "LWP3 Hub '%s':", this->hub_name_.c_str());
  ESP_LOGCONFIG(TAG, "  MAC Address: %s", this->parent()->address_str());
}

void LWP3Hub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                  esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "[%s] Connected", this->hub_name_.c_str());
      }
      break;
    }
    case ESP_GATTC_CLOSE_EVT: {
      ESP_LOGW(TAG, "[%s] Disconnected", this->hub_name_.c_str());
      this->handle_disconnected_();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle_search_complete_();
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.handle != this->char_handle_)
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] Failed to register for notify, status=%d", this->hub_name_.c_str(),
                 param->reg_for_notify.status);
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->ready_ = true;
      ESP_LOGI(TAG, "[%s] Ready", this->hub_name_.c_str());
      if (!this->battery_listeners_.empty()) {
        this->write_raw_(lwp3_protocol::encode_hub_property_enable_updates(lwp3_protocol::HUB_PROP_BATTERY_VOLTAGE));
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->handle_notify_(param->notify.value, param->notify.value_len);
      break;
    }
    default:
      break;
  }
}

void LWP3Hub::handle_search_complete_() {
  auto service_uuid = espbt::ESPBTUUID::from_raw(lwp3_protocol::service_uuid());
  auto char_uuid = espbt::ESPBTUUID::from_raw(lwp3_protocol::characteristic_uuid());
  auto *chr = this->parent()->get_characteristic(service_uuid, char_uuid);
  if (chr == nullptr) {
    ESP_LOGW(TAG, "[%s] LWP3 service/characteristic not found - is this really a LEGO Powered Up hub?",
             this->hub_name_.c_str());
    return;
  }
  // Cache the handle now: once every registered node reaches ESTABLISHED, the
  // parent BLEClient releases its GATT cache, so `chr` itself must not be
  // dereferenced again after this point.
  this->char_handle_ = chr->handle;
  auto status = this->parent()->register_for_notify(chr->handle);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "[%s] register_for_notify failed, status=%d", this->hub_name_.c_str(), status);
  }
}

std::string LWP3Hub::port_label_(uint8_t port) {
  return port < LWP3_MAX_PORTS ? std::string(1, static_cast<char>('A' + port)) : std::to_string(port);
}

bool LWP3Hub::has_listener_(uint8_t port) const {
  for (const auto &listener : this->port_listeners_) {
    if (listener.port == port)
      return true;
  }
  return false;
}

void LWP3Hub::handle_notify_(const uint8_t *data, uint16_t len) {
  switch (lwp3_protocol::message_type(data, len)) {
    case lwp3_protocol::MSG_TYPE_PORT_VALUE_SINGLE: {
      lwp3_protocol::PortValueEvent event;
      if (!lwp3_protocol::decode_port_value_single(data, len, event))
        return;
      for (const auto &listener : this->port_listeners_) {
        if (listener.port == event.port && listener.on_value)
          listener.on_value(event.payload, event.payload_len);
      }
      return;
    }
    case lwp3_protocol::MSG_TYPE_HUB_PROPERTIES: {
      lwp3_protocol::HubPropertyUpdate update;
      if (!lwp3_protocol::decode_hub_property_update(data, len, update))
        return;
      if (update.property == lwp3_protocol::HUB_PROP_BATTERY_VOLTAGE && update.payload_len >= 1) {
        ESP_LOGD(TAG, "[%s] Battery %u%%", this->hub_name_.c_str(), update.payload[0]);
        for (const auto &listener : this->battery_listeners_)
          listener.on_value(update.payload[0]);
      }
      return;
    }
    default:
      break;
  }

  lwp3_protocol::HubAttachedIoEvent io_event;
  if (!lwp3_protocol::decode_hub_attached_io(data, len, io_event)) {
    ESP_LOGV(TAG, "[%s] Ignoring notify, message_type=0x%02X", this->hub_name_.c_str(),
             lwp3_protocol::message_type(data, len));
    return;
  }

  if (io_event.port >= LWP3_MAX_PORT_IDS) {
    ESP_LOGD(TAG, "[%s] Hub Attached I/O on port %u (out of range), ignoring", this->hub_name_.c_str(),
             io_event.port);
    return;
  }

  if (io_event.attached) {
    this->attached_ports_ |= (uint64_t{1} << io_event.port);
  } else {
    this->attached_ports_ &= ~(uint64_t{1} << io_event.port);
  }
  const std::string label = this->port_label_(io_event.port);
  const bool is_motor_port = io_event.port < LWP3_MAX_PORTS;
  const bool declared = is_motor_port && this->declared_ports_[io_event.port];
  const bool listened = this->has_listener_(io_event.port);

  if (io_event.attached) {
    ESP_LOGI(TAG, "[%s] Port %s: device attached (io_type_id=0x%04X)", this->hub_name_.c_str(), label.c_str(),
             io_event.io_type_id);
    if (is_motor_port && !declared && !listened) {
      ESP_LOGW(TAG, "[%s] Port %s has a device attached but isn't declared in YAML `ports:`",
               this->hub_name_.c_str(), label.c_str());
    }
    // Activate each listened mode once per port (several binary_sensors can
    // share one button port), then let entities push any stored state.
    bool setup_sent = false;
    for (const auto &listener : this->port_listeners_) {
      if (listener.port != io_event.port)
        continue;
      if (!setup_sent) {
        this->write_command_(io_event.port, lwp3_protocol::encode_port_input_format_setup(
                                                io_event.port, listener.mode, static_cast<bool>(listener.on_value)));
        setup_sent = true;
      }
      if (listener.on_attach)
        listener.on_attach();
    }
  } else {
    ESP_LOGI(TAG, "[%s] Port %s: device detached", this->hub_name_.c_str(), label.c_str());
    if (declared) {
      ESP_LOGW(TAG, "[%s] Port %s is declared in YAML `ports:` but nothing is attached", this->hub_name_.c_str(),
               label.c_str());
    }
    if (is_motor_port && this->motors_[io_event.port] != nullptr) {
      this->motors_[io_event.port]->publish_state(0);
    }
    for (const auto &listener : this->port_listeners_) {
      if (listener.port == io_event.port && listener.on_reset)
        listener.on_reset();
    }
  }
}

void LWP3Hub::handle_disconnected_() {
  this->ready_ = false;
  this->char_handle_ = 0;
  this->attached_ports_ = 0;
  for (uint8_t port = 0; port < LWP3_MAX_PORTS; port++) {
    if (this->motors_[port] != nullptr) {
      this->motors_[port]->publish_state(0);
    }
  }
  for (const auto &listener : this->port_listeners_) {
    if (listener.on_reset)
      listener.on_reset();
  }
  for (const auto &listener : this->battery_listeners_)
    listener.on_reset();
}

void LWP3Hub::write_raw_(std::vector<uint8_t> payload) {
  if (!this->ready_) {
    ESP_LOGW(TAG, "[%s] Not connected yet, ignoring write", this->hub_name_.c_str());
    return;
  }
  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                         this->char_handle_, payload.size(), payload.data(),
                                         ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "[%s] Failed to write, status=%d", this->hub_name_.c_str(), status);
  }
}

void LWP3Hub::write_command_(uint8_t port, std::vector<uint8_t> payload) {
  const std::string label = this->port_label_(port);
  if (!this->ready_) {
    ESP_LOGW(TAG, "[%s] Not connected yet, ignoring command for port %s", this->hub_name_.c_str(), label.c_str());
    return;
  }
  if (!this->is_attached_(port)) {
    ESP_LOGW(TAG, "[%s] Port %s has no attached device, ignoring command", this->hub_name_.c_str(), label.c_str());
    return;
  }
  this->write_raw_(std::move(payload));
}

void LWP3Hub::set_motor_power(uint8_t port, int8_t power) {
  ESP_LOGD(TAG, "[%s] Port %s power -> %d", this->hub_name_.c_str(), this->port_label_(port).c_str(), power);
  this->write_command_(port, lwp3_protocol::encode_motor_power(port, power));
}

void LWP3Hub::set_led_rgb(uint8_t port, uint8_t r, uint8_t g, uint8_t b) {
  this->write_command_(port, lwp3_protocol::encode_led_rgb(port, r, g, b));
}

void LWP3Hub::play_sound(uint8_t port, lwp3_protocol::DuploSound sound) {
  // The speaker only reacts to WriteDirectModeData once its SOUND mode is the
  // port's active input format - without this activation step first, sound
  // commands are silently ignored by the hub firmware.
  this->write_command_(port, lwp3_protocol::encode_port_input_format_setup(port, lwp3_protocol::MODE_SOUND, true));
  this->write_command_(port, lwp3_protocol::encode_sound(port, sound));
}

}  // namespace lwp3_hub
}  // namespace esphome

#endif

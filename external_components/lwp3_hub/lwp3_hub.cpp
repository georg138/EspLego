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

void LWP3Hub::handle_notify_(const uint8_t *data, uint16_t len) {
  lwp3_protocol::HubAttachedIoEvent io_event;
  if (!lwp3_protocol::decode_hub_attached_io(data, len, io_event)) {
    ESP_LOGV(TAG, "[%s] Ignoring notify, message_type=0x%02X", this->hub_name_.c_str(),
             lwp3_protocol::message_type(data, len));
    return;
  }

  if (io_event.port >= LWP3_MAX_PORTS) {
    ESP_LOGD(TAG, "[%s] Hub Attached I/O on port %u (outside A-D range), ignoring", this->hub_name_.c_str(),
             io_event.port);
    return;
  }

  this->attached_ports_[io_event.port] = io_event.attached;
  const char port_letter = static_cast<char>('A' + io_event.port);
  const bool declared = this->declared_ports_[io_event.port];

  if (io_event.attached) {
    ESP_LOGI(TAG, "[%s] Port %c: device attached (io_type_id=0x%04X)", this->hub_name_.c_str(), port_letter,
             io_event.io_type_id);
    if (!declared) {
      ESP_LOGW(TAG, "[%s] Port %c has a device attached but isn't declared in YAML `ports:`",
               this->hub_name_.c_str(), port_letter);
    }
  } else {
    ESP_LOGI(TAG, "[%s] Port %c: device detached", this->hub_name_.c_str(), port_letter);
    if (declared) {
      ESP_LOGW(TAG, "[%s] Port %c is declared in YAML `ports:` but nothing is attached", this->hub_name_.c_str(),
               port_letter);
    }
    if (this->motors_[io_event.port] != nullptr) {
      this->motors_[io_event.port]->publish_state(0);
    }
  }
}

void LWP3Hub::handle_disconnected_() {
  this->ready_ = false;
  this->char_handle_ = 0;
  for (uint8_t port = 0; port < LWP3_MAX_PORTS; port++) {
    this->attached_ports_[port] = false;
    if (this->motors_[port] != nullptr) {
      this->motors_[port]->publish_state(0);
    }
  }
}

void LWP3Hub::write_command_(uint8_t port, std::vector<uint8_t> payload) {
  const char port_letter = static_cast<char>('A' + port);
  if (!this->ready_) {
    ESP_LOGW(TAG, "[%s] Not connected yet, ignoring command for port %c", this->hub_name_.c_str(), port_letter);
    return;
  }
  if (port >= LWP3_MAX_PORTS || !this->attached_ports_[port]) {
    ESP_LOGW(TAG, "[%s] Port %c has no attached device, ignoring command", this->hub_name_.c_str(), port_letter);
    return;
  }

  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                         this->char_handle_, payload.size(), payload.data(),
                                         ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "[%s] Failed to write command for port %c, status=%d", this->hub_name_.c_str(), port_letter,
             status);
  }
}

void LWP3Hub::set_motor_power(uint8_t port, int8_t power) {
  ESP_LOGD(TAG, "[%s] Port %c power -> %d", this->hub_name_.c_str(), static_cast<char>('A' + port), power);
  this->write_command_(port, lwp3_protocol::encode_motor_power(port, power));
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

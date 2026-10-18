#include "lwp3_protocol.h"

namespace esphome {
namespace lwp3_hub {
namespace lwp3_protocol {

uint8_t message_type(const uint8_t *data, size_t len) {
  if (len < 3)
    return 0xFF;
  return data[2];
}

namespace {

// Common header for every WriteDirectModeData Port Output Command: length
// (filled in by the caller once the payload is appended), hub ID, message
// type, port, startup/completion info, sub-command, mode.
std::vector<uint8_t> direct_mode_command_(uint8_t port, uint8_t mode) {
  return {
      0,  // length, filled in by the caller
      HUB_ID,
      MSG_TYPE_PORT_OUTPUT_COMMAND,
      port,
      static_cast<uint8_t>(STARTUP_EXECUTE_IMMEDIATELY | COMPLETION_COMMAND_FEEDBACK),
      SUBCMD_WRITE_DIRECT_MODE_DATA,
      mode,
  };
}

}  // namespace

std::vector<uint8_t> encode_motor_power(uint8_t port, int8_t power) {
  auto msg = direct_mode_command_(port, MODE_POWER);
  msg.push_back(static_cast<uint8_t>(power));
  msg[0] = static_cast<uint8_t>(msg.size());
  return msg;
}

std::vector<uint8_t> encode_sound(uint8_t port, DuploSound sound) {
  auto msg = direct_mode_command_(port, MODE_SOUND);
  msg.push_back(static_cast<uint8_t>(sound));
  msg[0] = static_cast<uint8_t>(msg.size());
  return msg;
}

std::vector<uint8_t> encode_led_rgb(uint8_t port, uint8_t r, uint8_t g, uint8_t b) {
  auto msg = direct_mode_command_(port, MODE_LED_RGB);
  msg.push_back(r);
  msg.push_back(g);
  msg.push_back(b);
  msg[0] = static_cast<uint8_t>(msg.size());
  return msg;
}

std::vector<uint8_t> encode_hub_property_enable_updates(uint8_t property) {
  std::vector<uint8_t> msg = {0, HUB_ID, MSG_TYPE_HUB_PROPERTIES, property, HUB_PROP_OP_ENABLE_UPDATES};
  msg[0] = static_cast<uint8_t>(msg.size());
  return msg;
}

std::vector<uint8_t> encode_port_input_format_setup(uint8_t port, uint8_t mode, bool enable_notifications) {
  std::vector<uint8_t> msg = {
      0,  // length, filled in below
      HUB_ID,
      MSG_TYPE_PORT_INPUT_FORMAT_SETUP_SINGLE,
      port,
      mode,
      0x01, 0x00, 0x00, 0x00,  // delta interval = 1 (uint32, little-endian)
      static_cast<uint8_t>(enable_notifications ? 0x01 : 0x00),
  };
  msg[0] = static_cast<uint8_t>(msg.size());
  return msg;
}

bool decode_hub_attached_io(const uint8_t *data, size_t len, HubAttachedIoEvent &out) {
  if (message_type(data, len) != MSG_TYPE_HUB_ATTACHED_IO || len < 5)
    return false;

  out.port = data[3];
  const uint8_t event = data[4];

  if (event == IO_EVENT_DETACHED) {
    out.attached = false;
    out.io_type_id = 0;
    return true;
  }

  if (event == IO_EVENT_ATTACHED || event == IO_EVENT_ATTACHED_VIRTUAL) {
    if (len < 7)
      return false;
    out.attached = true;
    out.io_type_id = static_cast<uint16_t>(data[5] | (static_cast<uint16_t>(data[6]) << 8));
    return true;
  }

  return false;
}

bool decode_port_value_single(const uint8_t *data, size_t len, PortValueEvent &out) {
  if (message_type(data, len) != MSG_TYPE_PORT_VALUE_SINGLE || len < 5)
    return false;
  out.port = data[3];
  out.payload = data + 4;
  out.payload_len = len - 4;
  return true;
}

bool decode_hub_property_update(const uint8_t *data, size_t len, HubPropertyUpdate &out) {
  if (message_type(data, len) != MSG_TYPE_HUB_PROPERTIES || len < 6 || data[4] != HUB_PROP_OP_UPDATE)
    return false;
  out.property = data[3];
  out.payload = data + 5;
  out.payload_len = len - 5;
  return true;
}

}  // namespace lwp3_protocol
}  // namespace lwp3_hub
}  // namespace esphome

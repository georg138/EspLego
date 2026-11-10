#include "lwp3_protocol.h"

#include <cstring>

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

std::vector<uint8_t> encode_port_input_format_setup(uint8_t port, uint8_t mode, bool enable_notifications,
                                                    uint32_t delta) {
  std::vector<uint8_t> msg = {
      0,  // length, filled in below
      HUB_ID,
      MSG_TYPE_PORT_INPUT_FORMAT_SETUP_SINGLE,
      port,
      mode,
      static_cast<uint8_t>(delta), static_cast<uint8_t>(delta >> 8),  // delta interval (uint32, little-endian)
      static_cast<uint8_t>(delta >> 16), static_cast<uint8_t>(delta >> 24),
      static_cast<uint8_t>(enable_notifications ? 0x01 : 0x00),
  };
  msg[0] = static_cast<uint8_t>(msg.size());
  return msg;
}

std::vector<uint8_t> encode_port_information_request(uint8_t port, uint8_t info_type) {
  return {5, HUB_ID, MSG_TYPE_PORT_INFORMATION_REQUEST, port, info_type};
}

std::vector<uint8_t> encode_port_mode_information_request(uint8_t port, uint8_t mode, uint8_t info_type) {
  return {6, HUB_ID, MSG_TYPE_PORT_MODE_INFORMATION_REQUEST, port, mode, info_type};
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

bool decode_port_mode_info(const uint8_t *data, size_t len, PortModeInfo &out) {
  // port, info type, capabilities, mode count, input modes (u16), output modes (u16)
  if (message_type(data, len) != MSG_TYPE_PORT_INFORMATION || len < 11 || data[4] != PORT_INFO_MODE_INFO)
    return false;
  out.port = data[3];
  out.capabilities = data[5];
  out.mode_count = data[6];
  out.input_modes = static_cast<uint16_t>(data[7] | (static_cast<uint16_t>(data[8]) << 8));
  out.output_modes = static_cast<uint16_t>(data[9] | (static_cast<uint16_t>(data[10]) << 8));
  return true;
}

bool decode_port_mode_name(const uint8_t *data, size_t len, PortModeName &out) {
  // port, mode, mode info type, then up to 11 name characters (zero-padded)
  if (message_type(data, len) != MSG_TYPE_PORT_MODE_INFORMATION || len < 6 || data[5] != MODE_INFO_NAME)
    return false;
  out.port = data[3];
  out.mode = data[4];
  out.name = reinterpret_cast<const char *>(data + 6);
  out.name_len = 0;
  while (6 + out.name_len < len && data[6 + out.name_len] != 0)
    out.name_len++;
  return true;
}

bool decode_port_mode_range(const uint8_t *data, size_t len, PortModeRange &out) {
  // port, mode, mode info type, then min and max as little-endian floats
  if (message_type(data, len) != MSG_TYPE_PORT_MODE_INFORMATION || len < 14 ||
      (data[5] != MODE_INFO_RAW && data[5] != MODE_INFO_SI))
    return false;
  out.port = data[3];
  out.mode = data[4];
  out.info_type = data[5];
  std::memcpy(&out.min, data + 6, sizeof(float));  // ESP32 is little-endian too
  std::memcpy(&out.max, data + 10, sizeof(float));
  return true;
}

bool decode_output_command_feedback(const uint8_t *data, size_t len, std::vector<OutputCommandFeedback> &out) {
  // Pairs of (port, feedback flags) after the 3-byte header.
  if (message_type(data, len) != MSG_TYPE_PORT_OUTPUT_COMMAND_FEEDBACK || len < 5)
    return false;
  out.clear();
  for (size_t i = 3; i + 1 < len; i += 2)
    out.push_back({data[i], data[i + 1]});
  return true;
}

}  // namespace lwp3_protocol
}  // namespace lwp3_hub
}  // namespace esphome

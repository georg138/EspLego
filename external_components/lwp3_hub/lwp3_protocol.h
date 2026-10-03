#pragma once

// Pure LEGO Wireless Protocol v3 (LWP3) byte-layout encode/decode helpers.
// No ESPHome or BLE includes here on purpose, so this stays independently
// testable and reusable outside of the esp32_ble_client plumbing.
//
// Reference: https://lego.github.io/lego-ble-wireless-protocol-docs/
// Byte layouts below were verified against that official spec, not ported
// from any other LWP3 implementation.

#include <cstdint>
#include <cstddef>
#include <vector>

namespace esphome {
namespace lwp3_hub {
namespace lwp3_protocol {

// GATT service/characteristic used by every LWP3 hub. Single characteristic:
// write-without-response for outgoing commands, notify for incoming events.
inline const char *service_uuid() { return "00001623-1212-efde-1623-785feabcd123"; }
inline const char *characteristic_uuid() { return "00001624-1212-efde-1623-785feabcd123"; }

constexpr uint8_t HUB_ID = 0x00;

// Message types (byte 2 of every message).
constexpr uint8_t MSG_TYPE_HUB_ATTACHED_IO = 0x04;
constexpr uint8_t MSG_TYPE_PORT_INPUT_FORMAT_SETUP_SINGLE = 0x41;
constexpr uint8_t MSG_TYPE_PORT_VALUE_SINGLE = 0x45;
constexpr uint8_t MSG_TYPE_PORT_OUTPUT_COMMAND = 0x81;

// Hub Attached I/O (0x04) event byte (byte 4).
constexpr uint8_t IO_EVENT_DETACHED = 0x00;
constexpr uint8_t IO_EVENT_ATTACHED = 0x01;
constexpr uint8_t IO_EVENT_ATTACHED_VIRTUAL = 0x02;

// Port Output Command (0x81) startup/completion info byte (byte 4).
constexpr uint8_t STARTUP_EXECUTE_IMMEDIATELY = 0x10;
constexpr uint8_t COMPLETION_COMMAND_FEEDBACK = 0x01;

// Port Output Command sub-command (byte 5) for direct mode writes.
constexpr uint8_t SUBCMD_WRITE_DIRECT_MODE_DATA = 0x51;
// Direct mode data mode byte (byte 6): mode 0 is power/PWM on every motor type.
constexpr uint8_t MODE_POWER = 0x00;
// Direct mode data mode byte for the Duplo Train Base's built-in speaker:
// mode 1 plays one of a fixed set of predefined sounds (payload = sound ID).
constexpr uint8_t MODE_SOUND = 0x01;

// Duplo Train Base predefined sound IDs (payload byte for MODE_SOUND).
enum class DuploSound : uint8_t {
  BRAKE = 3,
  STATION_DEPARTURE = 5,
  WATER_REFILL = 7,
  HORN = 9,
  STEAM = 10,
};

/// Returns byte 2 (message type) of a received LWP3 message, or 0xFF if too short to have one.
uint8_t message_type(const uint8_t *data, size_t len);

/// Builds a full Port Output Command message that sets a motor's power.
/// `power` is signed, -100..100 (0 stops the motor). Returned vector includes
/// the leading length byte, ready to write to the LWP3 characteristic.
std::vector<uint8_t> encode_motor_power(uint8_t port, int8_t power);

/// Builds a full Port Output Command message that plays a predefined sound on
/// a Duplo Train Base's built-in speaker. Returned vector includes the
/// leading length byte, ready to write to the LWP3 characteristic.
std::vector<uint8_t> encode_sound(uint8_t port, DuploSound sound);

/// Builds a Port Input Format Setup (Single) message that makes `mode` the
/// active input/output mode for a port. Some devices (e.g. the Duplo Train
/// Base speaker) silently ignore WriteDirectModeData commands for a mode
/// until it's been activated this way first.
std::vector<uint8_t> encode_port_input_format_setup(uint8_t port, uint8_t mode, bool enable_notifications);

struct HubAttachedIoEvent {
  uint8_t port{0};
  bool attached{false};
  uint16_t io_type_id{0};
};

/// Parses a Hub Attached I/O (0x04) message. Returns false if `data`/`len`
/// don't look like a well-formed message of this type.
bool decode_hub_attached_io(const uint8_t *data, size_t len, HubAttachedIoEvent &out);

}  // namespace lwp3_protocol
}  // namespace lwp3_hub
}  // namespace esphome

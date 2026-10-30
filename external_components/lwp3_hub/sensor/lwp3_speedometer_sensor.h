#pragma once

#ifdef USE_ESP32

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../lwp3_hub.h"

namespace esphome {
namespace lwp3_hub {

/// Duplo Train Base wheel speed (internal speedometer, port 0x13, mode 0).
/// Payload: signed 16-bit little-endian, raw units, 0 when the wheels are still.
/// Used by the dev config to detect a stalled train (see train-controller-dev.yaml).
class LWP3SpeedometerSensor : public sensor::Sensor, public Component, public Parented<LWP3Hub> {
 public:
  void setup() override;
  void dump_config() override;
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

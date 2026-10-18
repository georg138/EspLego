#pragma once

#ifdef USE_ESP32

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../lwp3_hub.h"

namespace esphome {
namespace lwp3_hub {

/// Hub battery level in percent (Hub Properties battery update).
class LWP3BatterySensor : public sensor::Sensor, public Component, public Parented<LWP3Hub> {
 public:
  void setup() override;
  void dump_config() override;
};

}  // namespace lwp3_hub
}  // namespace esphome

#endif

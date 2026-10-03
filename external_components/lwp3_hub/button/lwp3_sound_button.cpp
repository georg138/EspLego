#include "lwp3_sound_button.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace lwp3_hub {

static const char *const TAG = "lwp3_hub.button";

void LWP3SoundButton::dump_config() { LOG_BUTTON("", "LWP3 Sound Button", this); }

void LWP3SoundButton::press_action() { this->parent_->play_sound(this->port_, this->sound_); }

}  // namespace lwp3_hub
}  // namespace esphome

#endif

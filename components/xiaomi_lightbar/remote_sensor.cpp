#include "remote_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace xiaomi_lightbar {

static const char *const TAG = "xiaomi_lightbar.remote";

void XiaomiLightbarRemote::setup() {
  if (this->hub_ != nullptr)
    this->hub_->register_remote(this);
}

void XiaomiLightbarRemote::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Light Bar Remote:");
  ESP_LOGCONFIG(TAG, "  Serial: 0x%06X", this->serial_);
}

void XiaomiLightbarRemote::handle_command(uint8_t command, uint8_t options) {
  const char *action = nullptr;
  switch (command) {
    case CMD_ON_OFF:
      action = "press";
      break;
    case CMD_BRIGHTER:
      action = "turn_clockwise";
      break;
    case CMD_DIMMER:
      action = "turn_counterclockwise";
      break;
    case CMD_WARMER:
      action = "press_turn_counterclockwise";
      break;
    case CMD_COOLER:
      action = "press_turn_clockwise";
      break;
    case CMD_RESET:
      action = "hold";
      break;
    default:
      ESP_LOGD(TAG, "Unknown command 0x%02X (opts=0x%02X)", command, options);
      return;
  }
  ESP_LOGD(TAG, "Remote 0x%06X → %s", this->serial_, action);
  this->publish_state(action);
}

}  // namespace xiaomi_lightbar
}  // namespace esphome

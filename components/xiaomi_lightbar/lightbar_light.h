#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

#include "xiaomi_lightbar.h"

namespace esphome {
namespace xiaomi_lightbar {

class XiaomiLightbarLight : public Component, public light::LightOutput {
 public:
  void set_hub(XiaomiLightbarHub *hub) { hub_ = hub; }
  void set_serial(uint32_t serial) { serial_ = serial; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

  // Exposed for the `pair` button action.
  void send_pair();

 protected:
  // Map an absolute brightness (0..15) by first driving the bar to min via
  // DIMMER(-16) then BRIGHTER(N). Same trick for color temperature.
  void apply_brightness_(uint8_t target);
  void apply_color_temp_(uint8_t target);

  XiaomiLightbarHub *hub_{nullptr};
  uint32_t serial_{0};

  bool current_on_{true};
  uint8_t current_brightness_{15};  // 0..15
  uint8_t current_color_temp_{0};   // 0..15, 0 = coolest
  bool initialized_{false};
};

}  // namespace xiaomi_lightbar
}  // namespace esphome

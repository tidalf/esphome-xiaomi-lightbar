#include "lightbar_light.h"

#include "esphome/core/log.h"

namespace esphome {
namespace xiaomi_lightbar {

static const char *const TAG = "xiaomi_lightbar.light";

void XiaomiLightbarLight::setup() {
  if (this->hub_ != nullptr)
    this->hub_->register_light(this);
}

void XiaomiLightbarLight::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Light Bar:");
  ESP_LOGCONFIG(TAG, "  Serial: 0x%06X", this->serial_);
}

light::LightTraits XiaomiLightbarLight::get_traits() {
  light::LightTraits traits;
  traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
  traits.set_min_mireds(153.0f);
  traits.set_max_mireds(370.0f);
  return traits;
}

void XiaomiLightbarLight::send_pair() {
  if (this->hub_ == nullptr)
    return;
  // Same opcode as RESET in the protocol; the bar must have been power-cycled
  // within the last 10 s for pairing to take effect.
  this->hub_->send_command(this->serial_, CMD_RESET, 0x00);
}

void XiaomiLightbarLight::apply_brightness_(uint8_t target) {
  if (target > 15)
    target = 15;
  // Force to 0, then step up.
  this->hub_->send_command(this->serial_, CMD_DIMMER,
                           static_cast<uint8_t>(-16));
  if (target > 0)
    this->hub_->send_command(this->serial_, CMD_BRIGHTER, target);
  this->current_brightness_ = target;
}

void XiaomiLightbarLight::apply_color_temp_(uint8_t target) {
  if (target > 15)
    target = 15;
  // Force to coolest, then step warmer.
  this->hub_->send_command(this->serial_, CMD_COOLER,
                           static_cast<uint8_t>(-16));
  if (target > 0)
    this->hub_->send_command(this->serial_, CMD_WARMER, target);
  this->current_color_temp_ = target;
}

void XiaomiLightbarLight::write_state(light::LightState *state) {
  if (this->hub_ == nullptr)
    return;

  auto values = state->current_values;

  bool target_on = values.is_on();

  // Brightness float 0..1 → 0..15 (round, then clamp)
  float bf = values.get_brightness();
  if (bf < 0.0f) bf = 0.0f;
  if (bf > 1.0f) bf = 1.0f;
  uint8_t target_brightness = static_cast<uint8_t>(bf * 15.0f + 0.5f);

  // Color temp float in mireds → 0..15 (0 = cool, 15 = warm).
  // Match lightbar2mqtt's mapping exactly.
  float mireds = values.get_color_temperature();
  if (mireds < 153.0f) mireds = 153.0f;
  if (mireds > 370.0f) mireds = 370.0f;
  float amount = ((1.0f - ((mireds - 153.0f) / (370.0f - 153.0f))) * 15.0f) + 0.5f;
  uint8_t target_temp = static_cast<uint8_t>(amount);
  if (target_temp > 15) target_temp = 15;

  if (!this->initialized_) {
    // First call after boot — push everything to a known state so HA's stored
    // state matches the bar. We can't read back from the bar; assume it was on.
    this->current_on_ = true;
    this->initialized_ = true;
  }

  // Power transitions
  if (target_on != this->current_on_) {
    this->hub_->send_command(this->serial_, CMD_ON_OFF, 0x00);
    this->current_on_ = target_on;
  }

  if (!target_on)
    return;

  if (target_brightness != this->current_brightness_)
    this->apply_brightness_(target_brightness);

  if (target_temp != this->current_color_temp_)
    this->apply_color_temp_(target_temp);
}

}  // namespace xiaomi_lightbar
}  // namespace esphome

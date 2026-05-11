#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "xiaomi_lightbar.h"

namespace esphome {
namespace xiaomi_lightbar {

class XiaomiLightbarRemote : public text_sensor::TextSensor, public Component {
 public:
  void set_hub(XiaomiLightbarHub *hub) { hub_ = hub; }
  void set_serial(uint32_t serial) { serial_ = serial; }
  uint32_t get_serial() const { return serial_; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Called by the hub when a packet from this remote is decoded.
  void handle_command(uint8_t command, uint8_t options);

 protected:
  XiaomiLightbarHub *hub_{nullptr};
  uint32_t serial_{0};
};

}  // namespace xiaomi_lightbar
}  // namespace esphome

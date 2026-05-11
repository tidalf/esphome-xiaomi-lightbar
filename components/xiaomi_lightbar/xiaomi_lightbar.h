#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"

#include "nrf24.h"

namespace esphome {
namespace xiaomi_lightbar {

class XiaomiLightbarLight;
class XiaomiLightbarRemote;

enum LightbarCommand : uint8_t {
  CMD_ON_OFF = 0x01,
  CMD_COOLER = 0x02,
  CMD_WARMER = 0x03,
  CMD_BRIGHTER = 0x04,
  CMD_DIMMER = 0x05,
  CMD_RESET = 0x06,
};

class XiaomiLightbarHub : public Component,
                          public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                                spi::CLOCK_POLARITY_LOW,
                                                spi::CLOCK_PHASE_LEADING,
                                                spi::DATA_RATE_4MHZ> {
 public:
  void set_ce_pin(GPIOPin *pin) { ce_pin_ = pin; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Called by light platforms.
  void register_light(XiaomiLightbarLight *light);
  // Called by text_sensor platforms.
  void register_remote(XiaomiLightbarRemote *remote);

  // Build and transmit a command packet. Repeats 20x with 10 ms gap, matching
  // lightbar2mqtt's send loop.
  void send_command(uint32_t serial, uint8_t command, uint8_t options);

 protected:
  // CRC16 over `len` bytes, poly 0x1021, init 0xFFFE, no reflect, no xor-out.
  static uint16_t crc16_(const uint8_t *data, size_t len);

  void handle_received_packet_();
  uint8_t &package_id_for_(uint32_t serial);

  GPIOPin *ce_pin_{nullptr};
  NRF24 radio_;

  std::vector<XiaomiLightbarLight *> lights_;
  std::vector<XiaomiLightbarRemote *> remotes_;

  struct SerialSeq {
    uint32_t serial;
    uint8_t package_id{0};
    bool never_read{true};
  };
  std::vector<SerialSeq> seqs_;

  // 8-byte fixed packet preamble (lamperez's reverse-engineered value).
  static constexpr uint8_t preamble_[8] = {0x53, 0x39, 0x14, 0xDD,
                                           0x1C, 0x49, 0x34, 0x12};
};

}  // namespace xiaomi_lightbar
}  // namespace esphome

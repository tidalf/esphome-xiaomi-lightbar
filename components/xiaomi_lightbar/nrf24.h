#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace xiaomi_lightbar {

// Minimal nRF24L01(+) driver tailored to the Xiaomi light-bar protocol:
//   channel 68, 2 Mbps, CRC off, auto-ack off, dynamic payloads off,
//   17-byte fixed payload, address width 5.
class NRF24 {
 public:
  void set_ce_pin(GPIOPin *pin) { ce_pin_ = pin; }

  // SPI access is delegated to the owning SPIDevice via these callbacks.
  using SpiTransfer = std::function<void(uint8_t *tx_rx, size_t len)>;
  void set_spi_transfer(SpiTransfer fn) { spi_transfer_ = std::move(fn); }

  // Hardware setup matching lightbar2mqtt's Radio::setup().
  // Returns false if the chip does not respond (CONFIG read-back mismatch).
  bool setup();

  // TX one 17-byte payload. Caller handles the 20x repeat loop.
  void send_payload(const uint8_t *data, size_t len);

  // Switch radio to RX mode and hold CE high.
  void start_listening();
  void stop_listening();

  // True when RX FIFO is non-empty.
  bool available();

  // Read one payload (always reads `payload_size_` bytes from RX FIFO).
  void read_payload(uint8_t *buf, size_t len);

 private:
  enum : uint8_t {
    CMD_R_REGISTER = 0x00,
    CMD_W_REGISTER = 0x20,
    CMD_R_RX_PAYLOAD = 0x61,
    CMD_W_TX_PAYLOAD = 0xA0,
    CMD_FLUSH_TX = 0xE1,
    CMD_FLUSH_RX = 0xE2,
    CMD_NOP = 0xFF,
  };
  enum : uint8_t {
    REG_CONFIG = 0x00,
    REG_EN_AA = 0x01,
    REG_EN_RXADDR = 0x02,
    REG_SETUP_AW = 0x03,
    REG_SETUP_RETR = 0x04,
    REG_RF_CH = 0x05,
    REG_RF_SETUP = 0x06,
    REG_STATUS = 0x07,
    REG_RX_ADDR_P0 = 0x0A,
    REG_TX_ADDR = 0x10,
    REG_RX_PW_P0 = 0x11,
    REG_FIFO_STATUS = 0x17,
    REG_DYNPD = 0x1C,
    REG_FEATURE = 0x1D,
  };

  void write_register(uint8_t reg, uint8_t value);
  void write_register_multi(uint8_t reg, const uint8_t *value, size_t len);
  uint8_t read_register(uint8_t reg);
  void command(uint8_t cmd);
  uint8_t status();
  void ce_high_();
  void ce_low_();

  GPIOPin *ce_pin_{nullptr};
  SpiTransfer spi_transfer_;
  static constexpr uint8_t payload_size_ = 17;
};

}  // namespace xiaomi_lightbar
}  // namespace esphome

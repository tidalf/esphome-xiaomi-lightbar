#include "nrf24.h"

#include "esphome/core/log.h"

namespace esphome {
namespace xiaomi_lightbar {

static const char *const TAG = "xiaomi_lightbar.nrf24";

void NRF24::command(uint8_t cmd) {
  uint8_t buf[1] = {cmd};
  this->spi_transfer_(buf, 1);
}

void NRF24::write_register(uint8_t reg, uint8_t value) {
  uint8_t buf[2] = {static_cast<uint8_t>(CMD_W_REGISTER | (reg & 0x1F)), value};
  this->spi_transfer_(buf, 2);
}

void NRF24::write_register_multi(uint8_t reg, const uint8_t *value, size_t len) {
  uint8_t buf[1 + 8];
  buf[0] = CMD_W_REGISTER | (reg & 0x1F);
  for (size_t i = 0; i < len; i++)
    buf[1 + i] = value[i];
  this->spi_transfer_(buf, 1 + len);
}

uint8_t NRF24::read_register(uint8_t reg) {
  uint8_t buf[2] = {static_cast<uint8_t>(CMD_R_REGISTER | (reg & 0x1F)), 0xFF};
  this->spi_transfer_(buf, 2);
  return buf[1];
}

uint8_t NRF24::status() {
  uint8_t buf[1] = {CMD_NOP};
  this->spi_transfer_(buf, 1);
  return buf[0];
}

void NRF24::ce_high_() {
  if (this->ce_pin_ != nullptr)
    this->ce_pin_->digital_write(true);
}
void NRF24::ce_low_() {
  if (this->ce_pin_ != nullptr)
    this->ce_pin_->digital_write(false);
}

bool NRF24::setup() {
  this->ce_low_();
  delay(5);

  // Power down first, then configure with CRC + auto-ack OFF.
  // CONFIG: PWR_UP=0, PRIM_RX=0, EN_CRC=0, MASK_*=0
  this->write_register(REG_CONFIG, 0x00);
  delay(2);

  // 5-byte address width
  this->write_register(REG_SETUP_AW, 0x03);
  // Disable auto-ack on all pipes
  this->write_register(REG_EN_AA, 0x00);
  // Enable RX on pipe 0 only
  this->write_register(REG_EN_RXADDR, 0x01);
  // No auto-retransmit (we do it in software 20x)
  this->write_register(REG_SETUP_RETR, 0xFF);
  // Channel 68
  this->write_register(REG_RF_CH, 68);
  // RF_SETUP: 2 Mbps (RF_DR_HIGH=1, RF_DR_LOW=0), 0 dBm power, LNA_HCURR set
  // Bit 3 RF_DR_HIGH=1, bits 2:1 RF_PWR=11 (0 dBm), bit 0 LNA_HCURR=1
  this->write_register(REG_RF_SETUP, 0x0F);
  // Fixed payload size 17
  this->write_register(REG_RX_PW_P0, payload_size_);
  // Dynamic payloads off
  this->write_register(REG_DYNPD, 0x00);
  this->write_register(REG_FEATURE, 0x00);

  // TX addr = 0x5555555555 (5 bytes, LSB first on the wire).
  uint8_t tx_addr[5] = {0x55, 0x55, 0x55, 0x55, 0x55};
  this->write_register_multi(REG_TX_ADDR, tx_addr, 5);
  // RX addr P0 = 0xAAAAAAAAAA
  uint8_t rx_addr[5] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
  this->write_register_multi(REG_RX_ADDR_P0, rx_addr, 5);

  // Clear status flags (RX_DR | TX_DS | MAX_RT)
  this->write_register(REG_STATUS, 0x70);
  this->command(CMD_FLUSH_RX);
  this->command(CMD_FLUSH_TX);

  // Sanity: read RF_CH back
  uint8_t ch = this->read_register(REG_RF_CH);
  if (ch != 68) {
    ESP_LOGE(TAG, "nRF24 not responding (RF_CH read-back %u)", ch);
    return false;
  }

  // Default to listening mode.
  this->start_listening();
  return true;
}

void NRF24::start_listening() {
  // CONFIG: PWR_UP=1, PRIM_RX=1, EN_CRC=0
  this->write_register(REG_CONFIG, 0x03);
  this->write_register(REG_STATUS, 0x70);
  this->command(CMD_FLUSH_RX);
  delay(2);
  this->ce_high_();
  delayMicroseconds(150);
}

void NRF24::stop_listening() {
  this->ce_low_();
  delayMicroseconds(200);
  // CONFIG: PWR_UP=1, PRIM_RX=0
  this->write_register(REG_CONFIG, 0x02);
  this->command(CMD_FLUSH_TX);
  delayMicroseconds(150);
}

void NRF24::send_payload(const uint8_t *data, size_t len) {
  // Load TX payload (no ack)
  uint8_t buf[1 + 32];
  buf[0] = CMD_W_TX_PAYLOAD;
  size_t n = len <= 32 ? len : 32;
  for (size_t i = 0; i < n; i++)
    buf[1 + i] = data[i];
  this->spi_transfer_(buf, 1 + n);

  // Pulse CE high to fire the transmitter.
  this->ce_high_();
  delayMicroseconds(15);
  this->ce_low_();

  // Wait briefly for TX_DS in STATUS (or MAX_RT, which can't happen here since
  // auto-ack is off — but we still bail out on timeout).
  uint32_t start = micros();
  while (true) {
    uint8_t s = this->status();
    if (s & 0x20) {  // TX_DS
      this->write_register(REG_STATUS, 0x20);
      break;
    }
    if (micros() - start > 2000) {
      // Give up — flush and continue. lightbar2mqtt fires 20x so this is fine.
      this->command(CMD_FLUSH_TX);
      this->write_register(REG_STATUS, 0x70);
      break;
    }
  }
}

bool NRF24::available() {
  uint8_t fifo = this->read_register(REG_FIFO_STATUS);
  return (fifo & 0x01) == 0;  // RX_EMPTY bit
}

void NRF24::read_payload(uint8_t *buf, size_t len) {
  // SPI buffer: [R_RX_PAYLOAD] [n bytes payload]
  uint8_t spi_buf[1 + payload_size_] = {0};
  spi_buf[0] = CMD_R_RX_PAYLOAD;
  this->spi_transfer_(spi_buf, 1 + payload_size_);
  size_t n = len <= payload_size_ ? len : payload_size_;
  for (size_t i = 0; i < n; i++)
    buf[i] = spi_buf[1 + i];
  // Clear RX_DR
  this->write_register(REG_STATUS, 0x40);
}

}  // namespace xiaomi_lightbar
}  // namespace esphome

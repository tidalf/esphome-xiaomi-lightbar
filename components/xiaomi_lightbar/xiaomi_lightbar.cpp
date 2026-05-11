#include "xiaomi_lightbar.h"

#include <cstring>

#include "esphome/core/log.h"

#include "lightbar_light.h"
#include "remote_sensor.h"

namespace esphome {
namespace xiaomi_lightbar {

static const char *const TAG = "xiaomi_lightbar";

constexpr uint8_t XiaomiLightbarHub::preamble_[8];

void XiaomiLightbarHub::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Xiaomi Light Bar hub...");

  if (this->ce_pin_ != nullptr) {
    this->ce_pin_->setup();
    this->ce_pin_->digital_write(false);
  }

  this->spi_setup();

  this->radio_.set_ce_pin(this->ce_pin_);
  this->radio_.set_spi_transfer([this](uint8_t *buf, size_t len) {
    this->enable();
    this->transfer_array(buf, len);
    this->disable();
  });

  if (!this->radio_.setup()) {
    ESP_LOGE(TAG, "nRF24 setup failed — check wiring (CE/CSN/SCK/MOSI/MISO).");
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "nRF24 ready (channel 68, 2 Mbps, 17-byte payload).");
}

void XiaomiLightbarHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Light Bar hub:");
  LOG_PIN("  CE pin: ", this->ce_pin_);
  ESP_LOGCONFIG(TAG, "  Lights: %u", (unsigned) this->lights_.size());
  ESP_LOGCONFIG(TAG, "  Remotes: %u", (unsigned) this->remotes_.size());

  const auto &d = this->radio_.diag();
  if (!d.ran) {
    ESP_LOGCONFIG(TAG, "  nRF24: setup() never ran (check power / SPI bus)");
  } else {
    ESP_LOGCONFIG(TAG,
                  "  nRF24: STATUS=0x%02X RF_CH=%u (want 68) CONFIG=0x%02X "
                  "SETUP_AW=0x%02X (want 0x03)",
                  d.status, d.rf_ch_readback, d.config_readback,
                  d.setup_aw_readback);
    if (d.rf_ch_readback == 0x00 && d.config_readback == 0x00 &&
        d.setup_aw_readback == 0x00) {
      ESP_LOGCONFIG(TAG,
                    "  → all-zero readback: MISO not returning data. "
                    "Check VCC=3V3, MISO wiring, and add a 10 uF cap across "
                    "the nRF24 VCC/GND.");
    } else if (d.rf_ch_readback == 0xFF && d.config_readback == 0xFF) {
      ESP_LOGCONFIG(TAG,
                    "  → all-0xFF readback: MISO floating. "
                    "Check MISO pin (GPIO20 for XIAO ESP32-C6) and CSN.");
    } else if (d.rf_ch_readback != 68) {
      ESP_LOGCONFIG(TAG,
                    "  → unexpected RF_CH readback: SPI is talking but "
                    "garbled — try shorter wires or lower SPI clock.");
    }
  }
}

void XiaomiLightbarHub::loop() {
  while (this->radio_.available())
    this->handle_received_packet_();
}

void XiaomiLightbarHub::register_light(XiaomiLightbarLight *light) {
  this->lights_.push_back(light);
}

void XiaomiLightbarHub::register_remote(XiaomiLightbarRemote *remote) {
  this->remotes_.push_back(remote);
  // Pre-register the seq counter so handle_received_packet_ has a slot.
  (void) this->package_id_for_(remote->get_serial());
}

uint8_t &XiaomiLightbarHub::package_id_for_(uint32_t serial) {
  for (auto &s : this->seqs_) {
    if (s.serial == serial)
      return s.package_id;
  }
  SerialSeq s;
  s.serial = serial;
  s.package_id = 0;
  s.never_read = true;
  this->seqs_.push_back(s);
  return this->seqs_.back().package_id;
}

// CRC16-CCITT variant used by the light-bar protocol:
//   poly 0x1021, init 0xFFFE, no input/output reflection, no final xor.
uint16_t XiaomiLightbarHub::crc16_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFE;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; b++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }
  }
  return crc;
}

void XiaomiLightbarHub::send_command(uint32_t serial, uint8_t command,
                                     uint8_t options) {
  uint8_t &seq = this->package_id_for_(serial);
  seq++;  // pre-increment, matches lightbar2mqtt

  uint8_t data[17] = {0};
  std::memcpy(data, preamble_, 8);
  data[8] = (serial >> 16) & 0xFF;
  data[9] = (serial >> 8) & 0xFF;
  data[10] = serial & 0xFF;
  data[11] = 0xFF;
  data[12] = seq;
  data[13] = command;
  data[14] = options;

  uint16_t crc = crc16_(data, 15);
  data[15] = (crc >> 8) & 0xFF;
  data[16] = crc & 0xFF;

  ESP_LOGD(TAG, "TX serial=0x%06X cmd=0x%02X opt=0x%02X seq=%u", serial,
           command, options, seq);

  this->radio_.stop_listening();
  for (int i = 0; i < 20; i++) {
    this->radio_.send_payload(data, sizeof(data));
    delay(10);
  }
  this->radio_.start_listening();
}

void XiaomiLightbarHub::handle_received_packet_() {
  // The nRF24 captures the OTA bytes shifted by 3 bits (CRC + auto-ack off,
  // address used as a sync word). Re-align exactly like lightbar2mqtt does.
  uint8_t raw[17] = {0};
  this->radio_.read_payload(raw, 17);

  uint8_t data[17];
  for (int i = 0; i < 17; i++) {
    if (i == 0) {
      data[i] = 0x50 | (raw[i] >> 5);
    } else {
      data[i] = static_cast<uint8_t>(((raw[i - 1] >> 1) & 0x0F) << 4) |
                static_cast<uint8_t>((raw[i - 1] & 0x01) << 3) |
                static_cast<uint8_t>(raw[i] >> 5);
    }
  }

  if (std::memcmp(data, preamble_, 8) != 0)
    return;

  uint16_t calc_crc = crc16_(data, 15);
  uint16_t pkt_crc = (uint16_t(data[15]) << 8) | data[16];
  if (calc_crc != pkt_crc) {
    ESP_LOGV(TAG, "RX bad CRC");
    return;
  }

  uint32_t serial =
      (uint32_t(data[8]) << 16) | (uint32_t(data[9]) << 8) | data[10];

  XiaomiLightbarRemote *remote = nullptr;
  for (auto *r : this->remotes_) {
    if (r->get_serial() == serial) {
      remote = r;
      break;
    }
  }
  if (remote == nullptr) {
    ESP_LOGD(TAG, "RX from unknown serial 0x%06X (cmd=0x%02X)", serial,
             data[13]);
    return;
  }

  // Sequence-id dedup: per-serial slot; drop replays of the same id and old ids
  // (modulo 256, with a 64-wide back window matching lightbar2mqtt).
  uint8_t pkt_id = data[12];
  SerialSeq *slot = nullptr;
  for (auto &s : this->seqs_) {
    if (s.serial == serial) {
      slot = &s;
      break;
    }
  }
  if (slot == nullptr)
    return;

  if (!slot->never_read) {
    uint8_t last = slot->package_id;
    if (pkt_id == last)
      return;
    uint8_t diff = static_cast<uint8_t>(last - pkt_id);
    if (diff != 0 && diff < 64)
      return;  // old packet
  }
  slot->never_read = false;
  slot->package_id = pkt_id;

  remote->handle_command(data[13], data[14]);

  // If a light is configured with the same serial as the remote, the physical
  // remote also drove the bar. Mirror its effect locally so HA's view of the
  // light stays in sync without us re-sending any radio command.
  for (auto *l : this->lights_) {
    if (l->get_serial() == serial) {
      l->mirror_remote_command(data[13], data[14]);
    }
  }
}

}  // namespace xiaomi_lightbar
}  // namespace esphome

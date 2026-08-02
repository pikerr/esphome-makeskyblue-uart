#include "makeskyblue_uart.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace makeskyblue_uart {

const char *const TAG = "makeskyblue_uart";

void MakeskyblueUART::setup() { this->rx_buffer_.reserve(64); }

void MakeskyblueUART::update() {
  this->send_status_poll_();
}

void MakeskyblueUART::send_status_poll_() {
  static uint8_t poll_idx = 0;
  uint8_t addrs[] = {0x00, 0x01, 0x07};
  uint8_t addr = addrs[poll_idx];
  poll_idx = (poll_idx + 1) % 3;

  uint8_t poll_cmd[6] = {0xAA, 0x55, addr, 0x00, 0x00, static_cast<uint8_t>((0x55 + addr) & 0xFF)};
  ESP_LOGD(TAG, "Sending status poll request [AA 55 %02X 00 00 %02X] (addr 0x%02X)",
           addr, poll_cmd[5], addr);
  this->write_array(poll_cmd, 6);
}

void MakeskyblueUART::read_register(uint8_t reg) {
  // Read Config Register: AA CB [reg] 00 00 00 [checksum]
  uint8_t cmd[7] = {0xAA, 0xCB, reg, 0x00, 0x00, 0x00, 0x00};
  uint8_t crc = 0;
  for (int i = 1; i < 6; i++) {
    crc += cmd[i];
  }
  cmd[6] = crc;
  ESP_LOGD(TAG, "Sending read request for config reg 0x%02X", reg);
  this->write_array(cmd, 7);
}

void MakeskyblueUART::write_register(uint8_t reg, float value) {
  // Write Config Register: AA CA [reg] [val_lo] [val_hi] 00 00 [checksum]
  // Register values in MakeSkyBlue UART are scaled by 1000
  uint16_t val_int = static_cast<uint16_t>(std::round(value * 1000.0f));
  uint8_t cmd[8] = {0xAA,
                    0xCA,
                    reg,
                    static_cast<uint8_t>(val_int & 0xFF),
                    static_cast<uint8_t>((val_int >> 8) & 0xFF),
                    0x00,
                    0x00,
                    0x00};

  uint8_t crc = 0;
  for (int i = 1; i < 7; i++) {
    crc += cmd[i];
  }
  cmd[7] = crc;

  ESP_LOGD(TAG, "Writing register 0x%02X value %.3f (raw: %u)", reg, value,
           val_int);
  this->write_array(cmd, 8);
}

void MakeskyblueUART::loop() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    this->rx_buffer_.push_back(byte);

    // Sync to start header 0xAA or 0x55
    if (this->rx_buffer_[0] != 0xAA && this->rx_buffer_[0] != 0x55) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    if (this->rx_buffer_.size() < 2) {
      continue;
    }

    if (this->rx_buffer_[0] == 0x55 && this->rx_buffer_[1] == 0xAA) {
      // 15-byte response frame: 55 AA 03 ...
      if (this->rx_buffer_.size() < 15)
        continue;

      // Validate CRC (sum of bytes 0..13)
      uint8_t crc = 0;
      for (int i = 0; i < 14; i++) {
        crc += this->rx_buffer_[i];
      }

      if (crc == this->rx_buffer_[14]) {
        ESP_LOGD(TAG, "Received frame (15 bytes): %s",
                 format_hex_pretty(this->rx_buffer_.data(), 15).c_str());
        this->parse_status_frame_15_(this->rx_buffer_.data());
      } else {
        ESP_LOGW(TAG, "15-byte CRC mismatch (calc: 0x%02X, frame: 0x%02X)",
                 crc, this->rx_buffer_[14]);
      }

      this->rx_buffer_.erase(this->rx_buffer_.begin(),
                             this->rx_buffer_.begin() + 15);

    } else if (this->rx_buffer_[0] == 0xAA) {
      uint8_t frame_type = this->rx_buffer_[1];

      if (frame_type == 0xBB) {
        // Status response frame expected length: 20 bytes
        if (this->rx_buffer_.size() < 20)
          continue;

        // Validate CRC (sum of bytes 1..18)
        uint8_t crc = 0;
        for (int i = 1; i < 19; i++) {
          crc += this->rx_buffer_[i];
        }

        if (crc == this->rx_buffer_[19]) {
          ESP_LOGD(TAG, "Received frame (20 bytes): %s",
                   format_hex_pretty(this->rx_buffer_.data(), 20).c_str());
          this->parse_status_frame_(this->rx_buffer_.data());
        } else {
          ESP_LOGW(TAG, "Status frame CRC mismatch (calc: 0x%02X, frame: 0x%02X)",
                   crc, this->rx_buffer_[19]);
        }

        this->rx_buffer_.erase(this->rx_buffer_.begin(),
                               this->rx_buffer_.begin() + 20);

      } else if (frame_type == 0xDA) {
        // Config response frame expected length: 8 bytes
        if (this->rx_buffer_.size() < 8)
          continue;

        // Validate CRC (sum of bytes 1..6)
        uint8_t crc = 0;
        for (int i = 1; i < 7; i++) {
          crc += this->rx_buffer_[i];
        }

        if (crc == this->rx_buffer_[7]) {
          ESP_LOGD(TAG, "Received frame (8 bytes): %s",
                   format_hex_pretty(this->rx_buffer_.data(), 8).c_str());
          this->parse_config_frame_(this->rx_buffer_.data());
        } else {
          ESP_LOGW(TAG, "Config frame CRC mismatch (calc: 0x%02X, frame: 0x%02X)",
                   crc, this->rx_buffer_[7]);
        }

        this->rx_buffer_.erase(this->rx_buffer_.begin(),
                               this->rx_buffer_.begin() + 8);

      } else {
        // Unknown frame type, discard first byte to resync
        ESP_LOGV(TAG, "Unknown frame type 0x%02X, discarding byte", frame_type);
        this->rx_buffer_.erase(this->rx_buffer_.begin());
      }
    } else {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
    }
  }

  // Timeout check (no valid status frame for > 5000 ms)
  uint32_t now = millis();
  if (this->last_frame_time_ != 0 && (now - this->last_frame_time_ > 5000)) {
    ESP_LOGW(TAG, "Connection timeout (no status frames from controller)");
    this->last_frame_time_ = 0;

#ifdef USE_BINARY_SENSOR
    if (this->link_connected_binary_sensor_) {
      this->link_connected_binary_sensor_->publish_state(false);
    }
#endif
#ifdef USE_SENSOR
    if (this->battery_voltage_sensor_)
      this->battery_voltage_sensor_->publish_state(NAN);
    if (this->battery_current_sensor_)
      this->battery_current_sensor_->publish_state(NAN);
    if (this->solar_voltage_sensor_)
      this->solar_voltage_sensor_->publish_state(NAN);
    if (this->solar_power_sensor_)
      this->solar_power_sensor_->publish_state(NAN);
    if (this->temperature_sensor_)
      this->temperature_sensor_->publish_state(NAN);
    if (this->accumulated_kwh_sensor_)
      this->accumulated_kwh_sensor_->publish_state(NAN);
#endif
  }
}

void MakeskyblueUART::parse_status_frame_(const uint8_t *frame) {
  this->last_frame_time_ = millis();

#ifdef USE_TEXT_SENSOR
  if (this->raw_frame_text_sensor_) {
    this->raw_frame_text_sensor_->publish_state(format_hex_pretty(frame, 20));
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (this->link_connected_binary_sensor_) {
    this->link_connected_binary_sensor_->publish_state(true);
  }
#endif

  // Parsing Little-Endian 16-bit integers
  uint16_t batt_v_raw = frame[2] | (frame[3] << 8);
  uint16_t batt_i_raw = frame[4] | (frame[5] << 8);
  uint16_t pv_v_raw = frame[6] | (frame[7] << 8);
  uint16_t pv_w_raw = frame[8] | (frame[9] << 8);
  int16_t temp_raw = frame[10] | (frame[11] << 8);
  uint16_t kwh_raw = frame[12] | (frame[13] << 8);

  uint8_t mode_flags = frame[16];
  uint8_t error_flags = frame[17];

#ifdef USE_SENSOR
  if (this->battery_voltage_sensor_)
    this->battery_voltage_sensor_->publish_state(batt_v_raw * 0.1f);
  if (this->battery_current_sensor_)
    this->battery_current_sensor_->publish_state(batt_i_raw * 0.1f);
  if (this->solar_voltage_sensor_)
    this->solar_voltage_sensor_->publish_state(pv_v_raw * 0.1f);
  if (this->solar_power_sensor_)
    this->solar_power_sensor_->publish_state(pv_w_raw * 1.0f);
  if (this->temperature_sensor_)
    this->temperature_sensor_->publish_state(temp_raw * 0.1f);
  if (this->accumulated_kwh_sensor_)
    this->accumulated_kwh_sensor_->publish_state(kwh_raw * 1.0f);
#endif

#ifdef USE_BINARY_SENSOR
  if (this->mppt_mode_binary_sensor_) {
    // Bit 2: MPPT Mode Active
    this->mppt_mode_binary_sensor_->publish_state((mode_flags & (1 << 2)) != 0);
  }
  if (this->battery_undervoltage_binary_sensor_) {
    // Bit 0: Undervoltage
    this->battery_undervoltage_binary_sensor_->publish_state(
        (error_flags & (1 << 0)) != 0);
  }
  if (this->battery_overvoltage_binary_sensor_) {
    // Bit 1: Overvoltage
    this->battery_overvoltage_binary_sensor_->publish_state(
        (error_flags & (1 << 1)) != 0);
  }
#endif
}

void MakeskyblueUART::parse_config_frame_(const uint8_t *frame) {
  uint8_t reg = frame[2];
  uint16_t raw_val = frame[3] | (frame[4] << 8);
  float val = raw_val / 1000.0f;

  ESP_LOGD(TAG, "Received config register 0x%02X = %.3f", reg, val);

  switch (reg) {
  case REG_BULK_VOLTAGE:
#ifdef USE_SENSOR
    if (this->bulk_voltage_sensor_)
      this->bulk_voltage_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
    if (this->bulk_voltage_number_)
      this->bulk_voltage_number_->publish_state(val);
#endif
    break;
  case REG_FLOAT_VOLTAGE:
#ifdef USE_SENSOR
    if (this->float_voltage_sensor_)
      this->float_voltage_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
    if (this->float_voltage_number_)
      this->float_voltage_number_->publish_state(val);
#endif
    break;
  case REG_MAX_CHARGE_CURRENT:
#ifdef USE_SENSOR
    if (this->max_charge_current_sensor_)
      this->max_charge_current_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
    if (this->max_charge_current_number_)
      this->max_charge_current_number_->publish_state(val);
#endif
    break;
  case REG_UVP_OFF_VOLTAGE:
#ifdef USE_SENSOR
    if (this->uvp_off_voltage_sensor_)
      this->uvp_off_voltage_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
    if (this->uvp_off_voltage_number_)
      this->uvp_off_voltage_number_->publish_state(val);
#endif
    break;
  case REG_UVP_RECOVER_VOLTAGE:
#ifdef USE_SENSOR
    if (this->uvp_recover_voltage_sensor_)
      this->uvp_recover_voltage_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
    if (this->uvp_recover_voltage_number_)
      this->uvp_recover_voltage_number_->publish_state(val);
#endif
    break;
  case REG_BATTERY_TYPE:
#ifdef USE_SELECT
    if (this->battery_type_select_) {
      size_t idx = static_cast<size_t>(std::round(val));
      auto &options = this->battery_type_select_->traits.get_options();
      if (idx < options.size()) {
        this->battery_type_select_->publish_state(options[idx]);
      }
    }
#endif
    break;
  }
}

void MakeskyblueUART::parse_status_frame_15_(const uint8_t *frame) {
  this->last_frame_time_ = millis();

#ifdef USE_TEXT_SENSOR
  if (this->raw_frame_text_sensor_) {
    this->raw_frame_text_sensor_->publish_state(format_hex_pretty(frame, 15));
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (this->link_connected_binary_sensor_) {
    this->link_connected_binary_sensor_->publish_state(true);
  }
#endif

  // 15-byte status frame structure:
  // [0..1]: 55 AA (Header)
  // [2]: 03 (Command)
  // [3]: 07 (Register/Addr)
  // [4..5]: 00 08 (Data Length = 8 bytes)
  // [6..7]: PV Voltage (Little Endian, scale 0.1 V)
  // [8..9]: System Voltage Code (Big Endian: 4 = 48V, 3 = 36V, 2 = 24V, 1 = 12V)
  // [10..11]: Reserved (0x0000)
  // [12..13]: PV Power (Big Endian, scale 0.1 W)
  // [14]: Checksum (Sum of bytes 0..13)

  uint16_t pv_v_raw = frame[6] | (frame[7] << 8);
  uint16_t sys_code_raw = (frame[8] << 8) | frame[9];
  uint16_t pv_w_raw = (frame[12] << 8) | frame[13];

  float pv_v = pv_v_raw * 0.1f;
  float pv_w = pv_w_raw * 0.1f;

  float batt_v_nom = (sys_code_raw > 0 && sys_code_raw <= 4) ? (sys_code_raw * 12.0f) : 48.0f;
  float batt_i = (pv_w > 0.0f && batt_v_nom > 0.0f) ? (pv_w / batt_v_nom) : 0.0f;

#ifdef USE_SENSOR
  if (this->solar_voltage_sensor_)
    this->solar_voltage_sensor_->publish_state(pv_v);
  if (this->solar_power_sensor_)
    this->solar_power_sensor_->publish_state(pv_w);
  if (this->battery_voltage_sensor_)
    this->battery_voltage_sensor_->publish_state(batt_v_nom);
  if (this->battery_current_sensor_)
    this->battery_current_sensor_->publish_state(batt_i);
#endif

#ifdef USE_BINARY_SENSOR
  if (this->mppt_mode_binary_sensor_) {
    this->mppt_mode_binary_sensor_->publish_state(pv_w > 1.0f);
  }
#endif
}

void MakeskyblueUART::dump_config() {
  ESP_LOGCONFIG(TAG, "MakeSkyBlue UART:");
  this->check_uart_settings(9600);
}

} // namespace makeskyblue_uart
} // namespace esphome

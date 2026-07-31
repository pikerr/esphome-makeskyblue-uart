#include "makeskyblue_uart.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace makeskyblue_uart {

const char *const TAG = "makeskyblue_uart";

void MakeskyblueUART::setup() {
  this->rx_buffer_.reserve(64);
}

void MakeskyblueUART::update() {
  this->send_status_poll_();

  // Periodically query one config register to keep config entities in sync
  static const uint8_t config_regs[] = {
      REG_BULK_VOLTAGE, REG_FLOAT_VOLTAGE, REG_MAX_CHARGE_CURRENT,
      REG_UVP_OFF_VOLTAGE, REG_UVP_RECOVER_VOLTAGE, REG_BATTERY_TYPE};

  uint8_t reg_to_read = config_regs[this->config_poll_index_];
  this->config_poll_index_ = (this->config_poll_index_ + 1) % (sizeof(config_regs) / sizeof(config_regs[0]));

  // Delay config read slightly after status poll to avoid UART clutter
  this->set_timeout(100, [this, reg_to_read]() {
    this->read_register(reg_to_read);
  });
}

void MakeskyblueUART::send_status_poll_() {
  // Read Status Packet: AA 55 00 00 00 55
  uint8_t poll_cmd[6] = {0xAA, 0x55, 0x00, 0x00, 0x00, 0x55};
  ESP_LOGD(TAG, "Sending status poll request [AA 55 00 00 00 55]");
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
  uint8_t cmd[8] = {
      0xAA,
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

  ESP_LOGD(TAG, "Writing register 0x%02X value %.3f (raw: %u)", reg, value, val_int);
  this->write_array(cmd, 8);
}

void MakeskyblueUART::loop() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    this->rx_buffer_.push_back(byte);

    // Sync to start header 0xAA
    if (this->rx_buffer_[0] != 0xAA) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    if (this->rx_buffer_.size() < 2) {
      continue;
    }

    uint8_t frame_type = this->rx_buffer_[1];

    if (frame_type == 0xBB) {
      // Status response frame expected length: 20 bytes
      if (this->rx_buffer_.size() < 20) continue;

      // Validate CRC (sum of bytes 1..18)
      uint8_t crc = 0;
      for (int i = 1; i < 19; i++) {
        crc += this->rx_buffer_[i];
      }

      if (crc == this->rx_buffer_[19]) {
        ESP_LOGD(TAG, "Received valid status response frame (20 bytes)");
        this->parse_status_frame_(this->rx_buffer_.data());
      } else {
        ESP_LOGW(TAG, "Status frame CRC mismatch (calc: 0x%02X, frame: 0x%02X)", crc, this->rx_buffer_[19]);
      }

      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + 20);

    } else if (frame_type == 0xDA) {
      // Config response frame expected length: 8 bytes
      if (this->rx_buffer_.size() < 8) continue;

      // Validate CRC (sum of bytes 1..6)
      uint8_t crc = 0;
      for (int i = 1; i < 7; i++) {
        crc += this->rx_buffer_[i];
      }

      if (crc == this->rx_buffer_[7]) {
        ESP_LOGD(TAG, "Received valid config response frame (8 bytes)");
        this->parse_config_frame_(this->rx_buffer_.data());
      } else {
        ESP_LOGW(TAG, "Config frame CRC mismatch (calc: 0x%02X, frame: 0x%02X)", crc, this->rx_buffer_[7]);
      }

      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + 8);

    } else {
      // Unknown frame type, discard first byte to resync
      ESP_LOGV(TAG, "Unknown frame type 0x%02X, discarding byte", frame_type);
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
    if (this->battery_voltage_sensor_) this->battery_voltage_sensor_->publish_state(NAN);
    if (this->battery_current_sensor_) this->battery_current_sensor_->publish_state(NAN);
    if (this->solar_voltage_sensor_) this->solar_voltage_sensor_->publish_state(NAN);
    if (this->solar_power_sensor_) this->solar_power_sensor_->publish_state(NAN);
    if (this->temperature_sensor_) this->temperature_sensor_->publish_state(NAN);
    if (this->accumulated_kwh_sensor_) this->accumulated_kwh_sensor_->publish_state(NAN);
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
  uint16_t pv_v_raw   = frame[6] | (frame[7] << 8);
  uint16_t pv_w_raw   = frame[8] | (frame[9] << 8);
  int16_t  temp_raw   = frame[10] | (frame[11] << 8);
  uint16_t kwh_raw    = frame[12] | (frame[13] << 8);

  uint8_t mode_flags  = frame[16];
  uint8_t error_flags = frame[17];

#ifdef USE_SENSOR
  if (this->battery_voltage_sensor_) this->battery_voltage_sensor_->publish_state(batt_v_raw * 0.1f);
  if (this->battery_current_sensor_) this->battery_current_sensor_->publish_state(batt_i_raw * 0.1f);
  if (this->solar_voltage_sensor_)   this->solar_voltage_sensor_->publish_state(pv_v_raw * 0.1f);
  if (this->solar_power_sensor_)     this->solar_power_sensor_->publish_state(pv_w_raw * 1.0f);
  if (this->temperature_sensor_)     this->temperature_sensor_->publish_state(temp_raw * 0.1f);
  if (this->accumulated_kwh_sensor_) this->accumulated_kwh_sensor_->publish_state(kwh_raw * 1.0f);
#endif

#ifdef USE_BINARY_SENSOR
  if (this->mppt_mode_binary_sensor_) {
    // Bit 2: MPPT Mode Active
    this->mppt_mode_binary_sensor_->publish_state((mode_flags & (1 << 2)) != 0);
  }
  if (this->battery_undervoltage_binary_sensor_) {
    // Bit 0: Undervoltage
    this->battery_undervoltage_binary_sensor_->publish_state((error_flags & (1 << 0)) != 0);
  }
  if (this->battery_overvoltage_binary_sensor_) {
    // Bit 1: Overvoltage
    this->battery_overvoltage_binary_sensor_->publish_state((error_flags & (1 << 1)) != 0);
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
      if (this->bulk_voltage_sensor_) this->bulk_voltage_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
      if (this->bulk_voltage_number_) this->bulk_voltage_number_->publish_state(val);
#endif
      break;
    case REG_FLOAT_VOLTAGE:
#ifdef USE_SENSOR
      if (this->float_voltage_sensor_) this->float_voltage_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
      if (this->float_voltage_number_) this->float_voltage_number_->publish_state(val);
#endif
      break;
    case REG_MAX_CHARGE_CURRENT:
#ifdef USE_SENSOR
      if (this->max_charge_current_sensor_) this->max_charge_current_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
      if (this->max_charge_current_number_) this->max_charge_current_number_->publish_state(val);
#endif
      break;
    case REG_UVP_OFF_VOLTAGE:
#ifdef USE_SENSOR
      if (this->uvp_off_voltage_sensor_) this->uvp_off_voltage_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
      if (this->uvp_number_) this->uvp_off_voltage_number_->publish_state(val);
#endif
      break;
    case REG_UVP_RECOVER_VOLTAGE:
#ifdef USE_SENSOR
      if (this->uvp_recover_voltage_sensor_) this->uvp_recover_voltage_sensor_->publish_state(val);
#endif
#ifdef USE_NUMBER
      if (this->uvp_recover_voltage_number_) this->uvp_recover_voltage_number_->publish_state(val);
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
    default:
      break;
  }
}

void MakeskyblueUART::dump_config() {
  ESP_LOGCONFIG(TAG, "MakeSkyBlue UART:");
  this->check_uart_settings(9600);
}

}  // namespace makeskyblue_uart
}  // namespace esphome

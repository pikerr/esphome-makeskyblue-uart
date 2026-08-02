#include "makeskyblue_uart.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace esphome {
namespace makeskyblue_uart {

const char *const TAG = "makeskyblue_uart";

void MakeskyblueUART::setup() {
  this->rx_buffer_.reserve(64);
}

void MakeskyblueUART::update() {
  uint32_t now = millis();
  // Only send poll if no status frame received spontaneously for > 10 seconds
  if (this->last_frame_time_ == 0 || (now - this->last_frame_time_ > 10000)) {
    this->send_status_poll_();
  }
}

void MakeskyblueUART::send_status_poll_() {
  uint8_t poll_cmd[6] = {0xAA, 0x55, 0x07, 0x00, 0x00, 0x5C};
  ESP_LOGD(TAG, "Sending status poll request [AA 55 07 00 00 5C] (addr 0x07)");
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

void MakeskyblueUART::init_stream_server_() {
  this->server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (this->server_fd_ < 0) {
    ESP_LOGE(TAG, "Failed to create stream server socket: %d", errno);
    return;
  }

  int opt = 1;
  ::setsockopt(this->server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  int flags = ::fcntl(this->server_fd_, F_GETFL, 0);
  ::fcntl(this->server_fd_, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(this->stream_port_);

  if (::bind(this->server_fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    ESP_LOGE(TAG, "Failed to bind stream server socket to port %u: %d", this->stream_port_, errno);
    ::close(this->server_fd_);
    this->server_fd_ = -1;
    return;
  }

  if (::listen(this->server_fd_, 4) < 0) {
    ESP_LOGE(TAG, "Failed to listen on stream server socket: %d", errno);
    ::close(this->server_fd_);
    this->server_fd_ = -1;
    return;
  }

  ESP_LOGI(TAG, "Parallel stream server listening on TCP port %u", this->stream_port_);
}

void MakeskyblueUART::accept_stream_clients_() {
  if (this->server_fd_ < 0) return;

  struct sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);
  int client_fd = ::accept(this->server_fd_, (struct sockaddr *)&client_addr, &client_len);
  if (client_fd >= 0) {
    int flags = ::fcntl(client_fd, F_GETFL, 0);
    ::fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    this->client_fds_.push_back(client_fd);
    ESP_LOGI(TAG, "New stream client connected to port %u (fd: %d)", this->stream_port_, client_fd);
  }
}

void MakeskyblueUART::broadcast_stream_bytes_(const uint8_t *data, size_t len) {
  if (this->client_fds_.empty() || len == 0) return;

  for (auto it = this->client_fds_.begin(); it != this->client_fds_.end(); ) {
    ssize_t res = ::send(*it, data, len, MSG_DONTWAIT);
    if (res < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
      ESP_LOGI(TAG, "Stream client (fd: %d) disconnected", *it);
      ::close(*it);
      it = this->client_fds_.erase(it);
    } else {
      ++it;
    }
  }
}

void MakeskyblueUART::read_stream_clients_() {
  if (this->client_fds_.empty()) return;

  uint8_t buf[128];
  for (auto it = this->client_fds_.begin(); it != this->client_fds_.end(); ) {
    ssize_t bytes_read = ::recv(*it, buf, sizeof(buf), MSG_DONTWAIT);
    if (bytes_read > 0) {
      this->write_array(buf, bytes_read);
      ++it;
    } else if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
      ESP_LOGI(TAG, "Stream client (fd: %d) disconnected", *it);
      ::close(*it);
      it = this->client_fds_.erase(it);
    } else {
      ++it;
    }
  }
}

void MakeskyblueUART::loop() {
  if (this->stream_port_ > 0 && this->server_fd_ < 0 && network::is_connected()) {
    this->init_stream_server_();
  }

  this->accept_stream_clients_();
  this->read_stream_clients_();

  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    this->broadcast_stream_bytes_(&byte, 1);
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
      // Dynamic response frame starting with 55 AA
      if (this->rx_buffer_.size() < 6)
        continue;

      uint16_t data_len = (this->rx_buffer_[4] << 8) | this->rx_buffer_[5];
      size_t total_len = 6 + data_len + 1;

      if (this->rx_buffer_.size() < total_len)
        continue;

      // Validate CRC (sum of bytes 0 .. total_len - 2)
      uint8_t crc = 0;
      for (size_t i = 0; i < total_len - 1; i++) {
        crc += this->rx_buffer_[i];
      }

      if (crc == this->rx_buffer_[total_len - 1]) {
        ESP_LOGD(TAG, "Received frame (%zu bytes): %s", total_len,
                 format_hex_pretty(this->rx_buffer_.data(), total_len).c_str());
        this->parse_dynamic_frame_(this->rx_buffer_.data(), total_len);
      } else {
        ESP_LOGW(TAG, "Dynamic frame CRC mismatch (calc: 0x%02X, frame: 0x%02X)",
                 crc, this->rx_buffer_[total_len - 1]);
      }

      this->rx_buffer_.erase(this->rx_buffer_.begin(),
                             this->rx_buffer_.begin() + total_len);

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

  // Timeout check (no valid status frame for > 60000 ms / 60 seconds)
  uint32_t now = millis();
  if (this->last_frame_time_ != 0 && (now - this->last_frame_time_ > 60000)) {
    ESP_LOGW(TAG, "Connection timeout (no status frames from controller for 60s)");
    this->last_frame_time_ = 0;

#ifdef USE_BINARY_SENSOR
    if (this->link_connected_binary_sensor_) {
      this->link_connected_binary_sensor_->publish_state(false);
    }
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
  this->parse_dynamic_frame_(frame, 15);
}

void MakeskyblueUART::parse_dynamic_frame_(const uint8_t *frame, size_t length) {
  this->last_frame_time_ = millis();

#ifdef USE_TEXT_SENSOR
  if (this->raw_frame_text_sensor_) {
    this->raw_frame_text_sensor_->publish_state(format_hex_pretty(frame, length));
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (this->link_connected_binary_sensor_) {
    this->link_connected_binary_sensor_->publish_state(true);
  }
#endif

  uint8_t reg_id = frame[6];

  if (reg_id == 0x65 && length >= 11) {
    // Cumulative Generated Energy (kWh)
    uint32_t raw_val = (frame[8] << 16) | (frame[9] << 8) | frame[10];
    float kwh = raw_val / 100.0f;
#ifdef USE_SENSOR
    if (this->accumulated_kwh_sensor_)
      this->accumulated_kwh_sensor_->publish_state(kwh);
#endif
    ESP_LOGI(TAG, "Register 0x65 [Cumulative Energy]: %.2f kWh", kwh);
    return;
  }

  if (length >= 14) {
    uint16_t raw_val = (frame[12] << 8) | frame[13];
    float val = raw_val * 0.1f;
    uint8_t mode_flags = frame[10];
    uint8_t error_flags = frame[11];

    switch (reg_id) {
      case 0x66: // PV Solar Voltage
#ifdef USE_SENSOR
        if (this->solar_voltage_sensor_)
          this->solar_voltage_sensor_->publish_state(val);
#endif
        ESP_LOGI(TAG, "Register 0x66 [PV Voltage]: %.1f V", val);
        break;

      case 0x67: // Battery Voltage
        this->last_battery_voltage_ = val;
#ifdef USE_SENSOR
        if (this->battery_voltage_sensor_)
          this->battery_voltage_sensor_->publish_state(val);
#endif
        ESP_LOGI(TAG, "Register 0x67 [Battery Voltage]: %.1f V", val);
        break;

      case 0x68: // Load Current
        ESP_LOGI(TAG, "Register 0x68 [Load Current]: %.1f A", val);
        break;

      case 0x69: // Charge Current
        this->last_charge_current_ = val;
#ifdef USE_SENSOR
        if (this->battery_current_sensor_)
          this->battery_current_sensor_->publish_state(val);
#endif
        ESP_LOGI(TAG, "Register 0x69 [Charge Current]: %.1f A", val);

        // Calculate and publish Charge Power (P = U_bat * I_charge)
        if (this->last_battery_voltage_ > 0.0f) {
          float power_w = this->last_battery_voltage_ * val;
#ifdef USE_SENSOR
          if (this->solar_power_sensor_)
            this->solar_power_sensor_->publish_state(power_w);
#endif
          ESP_LOGI(TAG, "Calculated Charge Power: %.1f W", power_w);
        }
        break;

      case 0x71: // Controller Temperature
#ifdef USE_SENSOR
        if (this->temperature_sensor_)
          this->temperature_sensor_->publish_state(val);
#endif
        ESP_LOGI(TAG, "Register 0x71 [Controller Temp]: %.1f °C", val);
        break;

      default:
        ESP_LOGD(TAG, "Unhandled register 0x%02X, raw val: %u", reg_id, raw_val);
        break;
    }

#ifdef USE_BINARY_SENSOR
    if (this->mppt_mode_binary_sensor_) {
      // Bit 2: MPPT Mode Active or charging current > 0.1A
      this->mppt_mode_binary_sensor_->publish_state((mode_flags & (1 << 2)) != 0 || this->last_charge_current_ > 0.1f);
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
}

void MakeskyblueUART::dump_config() {
  ESP_LOGCONFIG(TAG, "MakeSkyBlue UART:");
  this->check_uart_settings(9600);
}

} // namespace makeskyblue_uart
} // namespace esphome

#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace makeskyblue_uart {

extern const char *const TAG;

// Registers for MakeSkyBlue MiniUSB UART protocol
enum ConfigRegister : uint8_t {
  REG_BULK_VOLTAGE = 0x01,
  REG_FLOAT_VOLTAGE = 0x02,
  REG_LOAD_TIMER = 0x03,
  REG_MAX_CHARGE_CURRENT = 0x04,
  REG_UVP_OFF_VOLTAGE = 0x05,
  REG_UVP_RECOVER_VOLTAGE = 0x06,
  REG_COM_ADDRESS = 0x07,
  REG_BATTERY_TYPE = 0x08,
  REG_BATTERY_CELLS = 0x09,
};

class MakeskyblueUARTNumber;
class MakeskyblueUARTSelect;

class MakeskyblueUART : public PollingComponent, public uart::UARTDevice {
public:
  MakeskyblueUART() = default;

  // Sensor setters
#ifdef USE_SENSOR
  void set_battery_voltage_sensor(sensor::Sensor *s) {
    this->battery_voltage_sensor_ = s;
  }
  void set_battery_current_sensor(sensor::Sensor *s) {
    this->battery_current_sensor_ = s;
  }
  void set_solar_voltage_sensor(sensor::Sensor *s) {
    this->solar_voltage_sensor_ = s;
  }
  void set_solar_power_sensor(sensor::Sensor *s) {
    this->solar_power_sensor_ = s;
  }
  void set_temperature_sensor(sensor::Sensor *s) {
    this->temperature_sensor_ = s;
  }
  void set_accumulated_kwh_sensor(sensor::Sensor *s) {
    this->accumulated_kwh_sensor_ = s;
  }
  void set_bulk_voltage_sensor(sensor::Sensor *s) {
    this->bulk_voltage_sensor_ = s;
  }
  void set_float_voltage_sensor(sensor::Sensor *s) {
    this->float_voltage_sensor_ = s;
  }
  void set_max_charge_current_sensor(sensor::Sensor *s) {
    this->max_charge_current_sensor_ = s;
  }
  void set_uvp_off_voltage_sensor(sensor::Sensor *s) {
    this->uvp_off_voltage_sensor_ = s;
  }
  void set_uvp_recover_voltage_sensor(sensor::Sensor *s) {
    this->uvp_recover_voltage_sensor_ = s;
  }
#endif

  // Binary sensor setters
#ifdef USE_BINARY_SENSOR
  void set_link_connected_binary_sensor(binary_sensor::BinarySensor *s) {
    this->link_connected_binary_sensor_ = s;
  }
  void set_mppt_mode_binary_sensor(binary_sensor::BinarySensor *s) {
    this->mppt_mode_binary_sensor_ = s;
  }
  void set_battery_undervoltage_binary_sensor(binary_sensor::BinarySensor *s) {
    this->battery_undervoltage_binary_sensor_ = s;
  }
  void set_battery_overvoltage_binary_sensor(binary_sensor::BinarySensor *s) {
    this->battery_overvoltage_binary_sensor_ = s;
  }
#endif

  // Number setters
#ifdef USE_NUMBER
  void set_bulk_voltage_number(MakeskyblueUARTNumber *n) {
    this->bulk_voltage_number_ = n;
  }
  void set_float_voltage_number(MakeskyblueUARTNumber *n) {
    this->float_voltage_number_ = n;
  }
  void set_max_charge_current_number(MakeskyblueUARTNumber *n) {
    this->max_charge_current_number_ = n;
  }
  void set_uvp_off_voltage_number(MakeskyblueUARTNumber *n) {
    this->uvp_off_voltage_number_ = n;
  }
  void set_uvp_recover_voltage_number(MakeskyblueUARTNumber *n) {
    this->uvp_recover_voltage_number_ = n;
  }
#endif

  // Select setters
#ifdef USE_SELECT
  void set_battery_type_select(MakeskyblueUARTSelect *s) {
    this->battery_type_select_ = s;
  }
#endif

  // Text sensor setters
#ifdef USE_TEXT_SENSOR
  void set_raw_frame_text_sensor(text_sensor::TextSensor *s) {
    this->raw_frame_text_sensor_ = s;
  }
#endif

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  void write_register(uint8_t reg, float value);
  void read_register(uint8_t reg);

protected:
  void send_status_poll_();
  void parse_status_frame_(const uint8_t *frame);
  void parse_status_frame_15_(const uint8_t *frame);
  void parse_config_frame_(const uint8_t *frame);

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_frame_time_{0};

#ifdef USE_SENSOR
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *battery_current_sensor_{nullptr};
  sensor::Sensor *solar_voltage_sensor_{nullptr};
  sensor::Sensor *solar_power_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *accumulated_kwh_sensor_{nullptr};
  sensor::Sensor *bulk_voltage_sensor_{nullptr};
  sensor::Sensor *float_voltage_sensor_{nullptr};
  sensor::Sensor *max_charge_current_sensor_{nullptr};
  sensor::Sensor *uvp_off_voltage_sensor_{nullptr};
  sensor::Sensor *uvp_recover_voltage_sensor_{nullptr};
#endif

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *link_connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *mppt_mode_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *battery_undervoltage_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *battery_overvoltage_binary_sensor_{nullptr};
#endif

#ifdef USE_NUMBER
  MakeskyblueUARTNumber *bulk_voltage_number_{nullptr};
  MakeskyblueUARTNumber *float_voltage_number_{nullptr};
  MakeskyblueUARTNumber *max_charge_current_number_{nullptr};
  MakeskyblueUARTNumber *uvp_off_voltage_number_{nullptr};
  MakeskyblueUARTNumber *uvp_recover_voltage_number_{nullptr};
#endif

#ifdef USE_SELECT
  MakeskyblueUARTSelect *battery_type_select_{nullptr};
#endif

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *raw_frame_text_sensor_{nullptr};
#endif
};

#ifdef USE_NUMBER
class MakeskyblueUARTNumber : public number::Number {
public:
  void set_parent(MakeskyblueUART *parent) { this->parent_ = parent; }
  void set_register(uint8_t reg) { this->reg_ = reg; }

protected:
  void control(float value) override {
    if (this->parent_ != nullptr) {
      this->parent_->write_register(this->reg_, value);
    }
  }

  MakeskyblueUART *parent_{nullptr};
  uint8_t reg_{0};
};
#endif

#ifdef USE_SELECT
class MakeskyblueUARTSelect : public select::Select {
public:
  void set_parent(MakeskyblueUART *parent) { this->parent_ = parent; }
  void set_register(uint8_t reg) { this->reg_ = reg; }

protected:
  void control(const std::string &value) override {
    if (this->parent_ == nullptr)
      return;
    auto &options = this->traits.get_options();
    for (size_t i = 0; i < options.size(); i++) {
      if (options[i] == value) {
        this->parent_->write_register(this->reg_, static_cast<float>(i));
        break;
      }
    }
  }

  MakeskyblueUART *parent_{nullptr};
  uint8_t reg_{0};
};
#endif

} // namespace makeskyblue_uart
} // namespace esphome

from collections.abc import Mapping

from esphome import codegen as cg, config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_KILOWATT_HOURS,
    UNIT_SECOND,
    UNIT_VOLT,
    UNIT_WATT,
)

from . import MakeskyblueUART

CONF_MAKESKYBLUE_UART_ID = "makeskyblue_uart_id"
CONF_BATTERY_VOLTAGE = "battery_voltage"
CONF_BATTERY_CURRENT = "battery_current"
CONF_SOLAR_VOLTAGE = "solar_voltage"
CONF_SOLAR_POWER = "solar_power"
CONF_TEMPERATURE = "temperature"
CONF_ACCUMULATED_KWH = "accumulated_kwh"
CONF_BULK_VOLTAGE = "bulk_voltage"
CONF_FLOAT_VOLTAGE = "float_voltage"
CONF_MAX_CHARGE_CURRENT = "max_charge_current"
CONF_UVP_OFF_VOLTAGE = "uvp_off_voltage"
CONF_UVP_RECOVER_VOLTAGE = "uvp_recover_voltage"
CONF_LAST_SEEN_SECONDS = "last_seen_seconds"
CONF_LAST_REGISTER = "last_register"

TYPES = {
    CONF_BATTERY_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_BATTERY_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_SOLAR_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_SOLAR_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_ACCUMULATED_KWH: sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    ),
    CONF_BULK_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_FLOAT_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_MAX_CHARGE_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_UVP_OFF_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_UVP_RECOVER_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_LAST_SEEN_SECONDS: sensor.sensor_schema(
        unit_of_measurement=UNIT_SECOND,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DURATION,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    CONF_LAST_REGISTER: sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAKESKYBLUE_UART_ID): cv.use_id(MakeskyblueUART),
    }
).extend({cv.Optional(type): schema for type, schema in TYPES.items()})


async def to_code(config: Mapping) -> None:
    parent = await cg.get_variable(config[CONF_MAKESKYBLUE_UART_ID])
    for key, schema in TYPES.items():
        if key in config:
            conf = config[key]
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(parent, f"set_{key}_sensor")(sens))

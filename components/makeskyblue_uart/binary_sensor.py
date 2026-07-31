from collections.abc import Mapping

from esphome import codegen as cg, config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
)

from . import MakeskyblueUART

CONF_MAKESKYBLUE_UART_ID = "makeskyblue_uart_id"
CONF_LINK_CONNECTED = "link_connected"
CONF_MPPT_MODE = "mppt_mode"
CONF_BATTERY_UNDERVOLTAGE = "battery_undervoltage"
CONF_BATTERY_OVERVOLTAGE = "battery_overvoltage"

TYPES = {
    CONF_LINK_CONNECTED: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY
    ),
    CONF_MPPT_MODE: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_RUNNING
    ),
    CONF_BATTERY_UNDERVOLTAGE: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM
    ),
    CONF_BATTERY_OVERVOLTAGE: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAKESKYBLUE_UART_ID): cv.use_id(MakeskyblueUART),
    }
).extend({cv.Optional(type): schema for type, schema in TYPES.items()})


async def to_code(config: Mapping) -> None:
    parent = await cg.get_variable(config[CONF_MAKESKYBLUE_UART_ID])
    for key in TYPES:
        if key in config:
            conf = config[key]
            sens = await binary_sensor.new_binary_sensor(conf)
            cg.add(getattr(parent, f"set_{key}_binary_sensor")(sens))

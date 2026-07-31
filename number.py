from collections.abc import Mapping

from esphome import codegen as cg, config_validation as cv
from esphome.components import number
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    UNIT_AMPERE,
    UNIT_VOLT,
)

from . import MakeskyblueUART, makeskyblue_uart_ns

CONF_MAKESKYBLUE_UART_ID = "makeskyblue_uart_id"
CONF_BULK_VOLTAGE = "bulk_voltage"
CONF_FLOAT_VOLTAGE = "float_voltage"
CONF_MAX_CHARGE_CURRENT = "max_charge_current"
CONF_UVP_OFF_VOLTAGE = "uvp_off_voltage"
CONF_UVP_RECOVER_VOLTAGE = "uvp_recover_voltage"

MakeskyblueUARTNumber = makeskyblue_uart_ns.class_(
    "MakeskyblueUARTNumber", number.Number
)

TYPES = {
    CONF_BULK_VOLTAGE: (
        0x01,
        number.number_schema(
            MakeskyblueUARTNumber,
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
        ),
    ),
    CONF_FLOAT_VOLTAGE: (
        0x02,
        number.number_schema(
            MakeskyblueUARTNumber,
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
        ),
    ),
    CONF_MAX_CHARGE_CURRENT: (
        0x04,
        number.number_schema(
            MakeskyblueUARTNumber,
            unit_of_measurement=UNIT_AMPERE,
            device_class=DEVICE_CLASS_CURRENT,
        ),
    ),
    CONF_UVP_OFF_VOLTAGE: (
        0x05,
        number.number_schema(
            MakeskyblueUARTNumber,
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
        ),
    ),
    CONF_UVP_RECOVER_VOLTAGE: (
        0x06,
        number.number_schema(
            MakeskyblueUARTNumber,
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
        ),
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAKESKYBLUE_UART_ID): cv.use_id(MakeskyblueUART),
    }
).extend({cv.Optional(type): schema[1] for type, schema in TYPES.items()})


async def to_code(config: Mapping) -> None:
    parent = await cv.use_id(MakeskyblueUART)(config[CONF_MAKESKYBLUE_UART_ID])
    for key, (reg_id, _) in TYPES.items():
        if key in config:
            conf = config[key]
            num = await number.new_number(
                conf,
                min_value=conf.get("min_value", 0.0),
                max_value=conf.get("max_value", 100.0),
                step=conf.get("step", 0.1),
            )
            cg.add(num.set_parent(parent))
            cg.add(num.set_register(reg_id))
            cg.add(getattr(parent, f"set_{key}_number")(num))

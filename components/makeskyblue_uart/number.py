from collections.abc import Mapping

from esphome import codegen as cg, config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
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


def make_number_schema(unit_of_measurement, device_class):
    return number.number_schema(
        MakeskyblueUARTNumber,
        unit_of_measurement=unit_of_measurement,
        device_class=device_class,
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE): cv.float_,
            cv.Optional(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP): cv.float_,
        }
    )


TYPES = {
    CONF_BULK_VOLTAGE: (
        0x01,
        make_number_schema(UNIT_VOLT, DEVICE_CLASS_VOLTAGE),
        10.0,
        65.0,
        0.1,
    ),
    CONF_FLOAT_VOLTAGE: (
        0x02,
        make_number_schema(UNIT_VOLT, DEVICE_CLASS_VOLTAGE),
        10.0,
        65.0,
        0.1,
    ),
    CONF_MAX_CHARGE_CURRENT: (
        0x04,
        make_number_schema(UNIT_AMPERE, DEVICE_CLASS_CURRENT),
        0.0,
        60.0,
        1.0,
    ),
    CONF_UVP_OFF_VOLTAGE: (
        0x05,
        make_number_schema(UNIT_VOLT, DEVICE_CLASS_VOLTAGE),
        9.0,
        60.0,
        0.1,
    ),
    CONF_UVP_RECOVER_VOLTAGE: (
        0x06,
        make_number_schema(UNIT_VOLT, DEVICE_CLASS_VOLTAGE),
        10.0,
        62.0,
        0.1,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAKESKYBLUE_UART_ID): cv.use_id(MakeskyblueUART),
    }
).extend({cv.Optional(type): schema_info[1] for type, schema_info in TYPES.items()})


async def to_code(config: Mapping) -> None:
    parent = await cg.get_variable(config[CONF_MAKESKYBLUE_UART_ID])
    for key, (reg_id, _, def_min, def_max, def_step) in TYPES.items():
        if key in config:
            conf = config[key]
            min_v = conf.get(CONF_MIN_VALUE, def_min)
            max_v = conf.get(CONF_MAX_VALUE, def_max)
            step_v = conf.get(CONF_STEP, def_step)
            num = await number.new_number(
                conf,
                min_value=min_v,
                max_value=max_v,
                step=step_v,
            )
            cg.add(num.set_parent(parent))
            cg.add(num.set_register(reg_id))
            cg.add(getattr(parent, f"set_{key}_number")(num))

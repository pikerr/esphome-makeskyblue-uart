from collections.abc import Mapping

from esphome import codegen as cg, config_validation as cv
from esphome.components import select

from . import MakeskyblueUART, makeskyblue_uart_ns

CONF_MAKESKYBLUE_UART_ID = "makeskyblue_uart_id"
CONF_BATTERY_TYPE = "battery_type"

MakeskyblueUARTSelect = makeskyblue_uart_ns.class_(
    "MakeskyblueUARTSelect", select.Select
)

DEFAULT_BATTERY_TYPES = ["SLA", "LiPo", "LiLo", "LiFe", "LiTo"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAKESKYBLUE_UART_ID): cv.use_id(MakeskyblueUART),
        cv.Optional(CONF_BATTERY_TYPE): select.select_schema(MakeskyblueUARTSelect),
    }
)


async def to_code(config: Mapping) -> None:
    parent = await cv.use_id(MakeskyblueUART)(config[CONF_MAKESKYBLUE_UART_ID])
    if CONF_BATTERY_TYPE in config:
        conf = config[CONF_BATTERY_TYPE]
        sel = await select.new_select(conf, options=DEFAULT_BATTERY_TYPES)
        cg.add(sel.set_parent(parent))
        cg.add(sel.set_register(0x08))
        cg.add(parent.set_battery_type_select(sel))

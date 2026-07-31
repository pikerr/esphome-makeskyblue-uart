from collections.abc import Mapping

from esphome import codegen as cg, config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = True

makeskyblue_uart_ns = cg.esphome_ns.namespace("makeskyblue_uart")
MakeskyblueUART = makeskyblue_uart_ns.class_(
    "MakeskyblueUART", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(MakeskyblueUART),
    })
    .extend(cv.polling_component_schema("2s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config: Mapping) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

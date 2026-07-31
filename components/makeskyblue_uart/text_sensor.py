from collections.abc import Mapping

from esphome import codegen as cg, config_validation as cv
from esphome.components import text_sensor

from . import MakeskyblueUART

CONF_MAKESKYBLUE_UART_ID = "makeskyblue_uart_id"
CONF_RAW_FRAME = "raw_frame"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MAKESKYBLUE_UART_ID): cv.use_id(MakeskyblueUART),
        cv.Optional(CONF_RAW_FRAME): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config: Mapping) -> None:
    parent = await cg.get_variable(config[CONF_MAKESKYBLUE_UART_ID])
    if CONF_RAW_FRAME in config:
        conf = config[CONF_RAW_FRAME]
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.set_raw_frame_text_sensor(sens))

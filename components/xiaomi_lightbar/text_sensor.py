import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import CONF_HUB_ID, CONF_SERIAL, XiaomiLightbarHub, xiaomi_lightbar_ns

DEPENDENCIES = ["xiaomi_lightbar"]

XiaomiLightbarRemote = xiaomi_lightbar_ns.class_(
    "XiaomiLightbarRemote", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(XiaomiLightbarRemote).extend(
    {
        cv.GenerateID(CONF_HUB_ID): cv.use_id(XiaomiLightbarHub),
        cv.Required(CONF_SERIAL): cv.hex_int_range(min=0, max=0xFFFFFF),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_HUB_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_serial(config[CONF_SERIAL]))

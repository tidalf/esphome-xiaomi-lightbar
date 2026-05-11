import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID

from . import CONF_HUB_ID, CONF_SERIAL, XiaomiLightbarHub, xiaomi_lightbar_ns

DEPENDENCIES = ["xiaomi_lightbar"]

XiaomiLightbarLight = xiaomi_lightbar_ns.class_(
    "XiaomiLightbarLight", cg.Component, light.LightOutput
)

CONFIG_SCHEMA = light.light_schema(
    XiaomiLightbarLight,
    light.LightType.BRIGHTNESS_ONLY,
).extend(
    {
        cv.GenerateID(CONF_HUB_ID): cv.use_id(XiaomiLightbarHub),
        cv.Required(CONF_SERIAL): cv.hex_int_range(min=0, max=0xFFFFFF),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    hub = await cg.get_variable(config[CONF_HUB_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_serial(config[CONF_SERIAL]))

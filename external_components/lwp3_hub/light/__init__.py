import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID

from .. import LWP3Hub, lwp3_hub_ns

CONF_LWP3_HUB_ID = "lwp3_hub_id"

LWP3RemoteLed = lwp3_hub_ns.class_(
    "LWP3RemoteLed", light.LightOutput, cg.Component, cg.Parented.template(LWP3Hub)
)

CONFIG_SCHEMA = (
    light.light_schema(LWP3RemoteLed, light.LightType.RGB)
    .extend({cv.GenerateID(CONF_LWP3_HUB_ID): cv.use_id(LWP3Hub)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_LWP3_HUB_ID])

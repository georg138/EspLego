import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, CONF_PORT

from .. import LWP3Hub, PORT_LETTERS, lwp3_hub_ns, validate_port_letter

CONF_LWP3_HUB_ID = "lwp3_hub_id"
CONF_MIN_POWER = "min_power"
CONF_LIGHT = "light"

LWP3MotorNumber = lwp3_hub_ns.class_(
    "LWP3MotorNumber", number.Number, cg.Component, cg.Parented.template(LWP3Hub)
)

CONFIG_SCHEMA = (
    number.number_schema(LWP3MotorNumber)
    .extend(
        {
            cv.GenerateID(CONF_LWP3_HUB_ID): cv.use_id(LWP3Hub),
            cv.Required(CONF_PORT): validate_port_letter,
            # Some motors (e.g. Duplo Train Base) need a minimum power before
            # they overcome static friction and actually move. 0 (default)
            # means no remapping - set this per-entity for motors that need it.
            cv.Optional(CONF_MIN_POWER, default=0): cv.int_range(min=0, max=100),
            # Port drives a Powered Up light instead of a motor: any nonzero
            # request means "on" (full brightness), direction is ignored, and
            # a manual brightness override (set_light_override) can take over.
            cv.Optional(CONF_LIGHT, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(var, config, min_value=-100, max_value=100, step=1)
    await cg.register_parented(var, config[CONF_LWP3_HUB_ID])
    cg.add(var.set_port(PORT_LETTERS[config[CONF_PORT]]))
    cg.add(var.set_min_power(config[CONF_MIN_POWER]))
    cg.add(var.set_light(config[CONF_LIGHT]))

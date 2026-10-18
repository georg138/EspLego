import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID

from .. import LWP3Hub, lwp3_hub_ns

CONF_LWP3_HUB_ID = "lwp3_hub_id"
CONF_GROUP = "group"
CONF_BUTTON = "button"

LWP3RemoteButton = lwp3_hub_ns.class_(
    "LWP3RemoteButton", binary_sensor.BinarySensor, cg.Component, cg.Parented.template(LWP3Hub)
)

lwp3_protocol_ns = lwp3_hub_ns.namespace("lwp3_protocol")

# The Remote's two button groups sit on fixed port IDs; each reports the
# pressed button as one byte (LEGO Wireless Protocol v3).
GROUPS = {
    "left": lwp3_protocol_ns.REMOTE_PORT_LEFT_BUTTONS,
    "right": lwp3_protocol_ns.REMOTE_PORT_RIGHT_BUTTONS,
}
BUTTONS = {
    "plus": lwp3_protocol_ns.REMOTE_BUTTON_PLUS,
    "red": lwp3_protocol_ns.REMOTE_BUTTON_RED,
    "minus": lwp3_protocol_ns.REMOTE_BUTTON_MINUS,
}

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(LWP3RemoteButton)
    .extend(
        {
            cv.GenerateID(CONF_LWP3_HUB_ID): cv.use_id(LWP3Hub),
            cv.Required(CONF_GROUP): cv.enum(GROUPS, lower=True),
            cv.Required(CONF_BUTTON): cv.enum(BUTTONS, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_parented(var, config[CONF_LWP3_HUB_ID])
    cg.add(var.set_port(config[CONF_GROUP]))
    cg.add(var.set_button_value(config[CONF_BUTTON]))

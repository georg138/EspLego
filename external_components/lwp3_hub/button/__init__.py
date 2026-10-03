import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, CONF_PORT

from .. import LWP3Hub, lwp3_hub_ns, validate_port_letter, PORT_LETTERS

CONF_LWP3_HUB_ID = "lwp3_hub_id"
CONF_SOUND = "sound"

lwp3_protocol_ns = lwp3_hub_ns.namespace("lwp3_protocol")
DuploSound = lwp3_protocol_ns.enum("DuploSound", is_class=True)

# Predefined Duplo Train Base speaker sounds. Values verified against the
# LEGO Wireless Protocol v3 WriteDirectModeData payload for the speaker's
# SOUND mode (mode 1) - not user-configurable numbers, just names for them.
SOUNDS = {
    "brake": DuploSound.BRAKE,
    "station_departure": DuploSound.STATION_DEPARTURE,
    "water_refill": DuploSound.WATER_REFILL,
    "horn": DuploSound.HORN,
    "steam": DuploSound.STEAM,
}

LWP3SoundButton = lwp3_hub_ns.class_(
    "LWP3SoundButton", button.Button, cg.Component, cg.Parented.template(LWP3Hub)
)

CONFIG_SCHEMA = (
    button.button_schema(LWP3SoundButton)
    .extend(
        {
            cv.GenerateID(CONF_LWP3_HUB_ID): cv.use_id(LWP3Hub),
            cv.Required(CONF_PORT): validate_port_letter,
            cv.Required(CONF_SOUND): cv.enum(SOUNDS, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
    await cg.register_parented(var, config[CONF_LWP3_HUB_ID])
    cg.add(var.set_port(PORT_LETTERS[config[CONF_PORT]]))
    cg.add(var.set_sound(config[CONF_SOUND]))

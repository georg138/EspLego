import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID, CONF_NAME

CODEOWNERS = ["@georg"]
DEPENDENCIES = ["ble_client"]
MULTI_CONF = True

lwp3_hub_ns = cg.esphome_ns.namespace("lwp3_hub")
LWP3Hub = lwp3_hub_ns.class_("LWP3Hub", cg.Component, ble_client.BLEClientNode)

CONF_PORTS = "ports"

# LEGO Powered Up port letters -> the numeric port IDs used on the wire by
# City/Train/Technic hubs (A=0 .. D=3).
PORT_LETTERS = {"A": 0, "B": 1, "C": 2, "D": 3}


def validate_port_letter(value):
    value = cv.string_strict(value).upper()
    if value not in PORT_LETTERS:
        raise cv.Invalid(f"port must be one of {', '.join(sorted(PORT_LETTERS))}")
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LWP3Hub),
            cv.Required(CONF_NAME): cv.string,
            cv.Required(CONF_PORTS): cv.ensure_list(validate_port_letter),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_hub_name(config[CONF_NAME]))
    for port in config[CONF_PORTS]:
        cg.add(var.add_declared_port(PORT_LETTERS[port]))

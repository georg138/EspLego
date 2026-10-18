import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from .. import LWP3Hub, lwp3_hub_ns

CONF_LWP3_HUB_ID = "lwp3_hub_id"

LWP3BatterySensor = lwp3_hub_ns.class_(
    "LWP3BatterySensor", sensor.Sensor, cg.Component, cg.Parented.template(LWP3Hub)
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        LWP3BatterySensor,
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend({cv.GenerateID(CONF_LWP3_HUB_ID): cv.use_id(LWP3Hub)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await cg.register_parented(var, config[CONF_LWP3_HUB_ID])

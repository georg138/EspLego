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

CONF_TYPE = "type"
TYPE_BATTERY = "battery"
TYPE_SPEEDOMETER = "speedometer"

LWP3BatterySensor = lwp3_hub_ns.class_(
    "LWP3BatterySensor", sensor.Sensor, cg.Component, cg.Parented.template(LWP3Hub)
)
LWP3SpeedometerSensor = lwp3_hub_ns.class_(
    "LWP3SpeedometerSensor", sensor.Sensor, cg.Component, cg.Parented.template(LWP3Hub)
)


def _schema(sensor_schema):
    return (
        sensor_schema.extend({cv.GenerateID(CONF_LWP3_HUB_ID): cv.use_id(LWP3Hub)})
        .extend(cv.COMPONENT_SCHEMA)
    )


# `type:` defaults to battery so existing configs keep working.
CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_BATTERY: _schema(
            sensor.sensor_schema(
                LWP3BatterySensor,
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            )
        ),
        # Duplo Train Base wheel speed (raw units until verified on hardware).
        TYPE_SPEEDOMETER: _schema(
            sensor.sensor_schema(
                LWP3SpeedometerSensor,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            )
        ),
    },
    default_type=TYPE_BATTERY,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await cg.register_parented(var, config[CONF_LWP3_HUB_ID])

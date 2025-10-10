from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_PIN_CLOCK,
    CONF_PIN_DATA,
    CONF_SCALE_FACTOR,
    ICON_METER_GAS,
    UNIT_GALLONS,
)

neptume_water_meter_ns = cg.esphome_ns.namespace("neptune_water_meter")

NeptuneWaterMeterSensor = neptume_water_meter_ns.class_(
    "NeptuneWaterMeterSensor", sensor.Sensor, cg.Component
)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        NeptuneWaterMeterSensor,
        unit_of_measurement=UNIT_GALLONS,
        icon=ICON_METER_GAS,
        accuracy_decimals=0,
    )
    .extend(
        {
            cv.Required(CONF_PIN_CLOCK): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Required(CONF_PIN_DATA): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Optional(CONF_SCALE_FACTOR, default=1): cv.float_,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    pin_clock = await cg.gpio_pin_expression(config[CONF_PIN_CLOCK])
    cg.add(var.set_pin_clock(pin_clock))
    pin_data = await cg.gpio_pin_expression(config[CONF_PIN_DATA])
    cg.add(var.set_pin_data(pin_data))
    cg.add(var.set_scale_factor(config[CONF_SCALE_FACTOR]))

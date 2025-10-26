from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_ENABLE_PIN,
    CONF_TIMEOUT,
    DEVICE_CLASS_WATER,
    STATE_CLASS_TOTAL_INCREASING,
)

CONF_ENABLE_COUNT = "enable_count"
UNIT_GALLONS = "gal"
ICON_METER_GAS = "mdi:meter-gas"

CODEOWNERS = ["@ngist"]
DEPENDENCIES = ["uart"]

neptume_water_meter_ns = cg.esphome_ns.namespace("neptune_water_meter")

NeptuneWaterMeterSensor = neptume_water_meter_ns.class_(
    "NeptuneWaterMeterSensor", sensor.Sensor, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        NeptuneWaterMeterSensor,
        unit_of_measurement=UNIT_GALLONS,
        icon=ICON_METER_GAS,
        accuracy_decimals=1,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        device_class=DEVICE_CLASS_WATER,
    )
    .extend(
        {
            cv.Required(CONF_CLOCK_PIN): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Required(CONF_ENABLE_PIN): cv.All(pins.internal_gpio_output_pin_schema),
            cv.Optional(CONF_TIMEOUT, default=1000): cv.int_,
            cv.Optional(CONF_ENABLE_COUNT, default=4): cv.int_,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "neptune_water_meter",
    require_tx=False,
    require_rx=True,
    # data_bits=8,
    # parity="NONE",
    # stop_bits=1,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    # If data pin is specified don't use UART use GPIO Mode instead
    if CONF_DATA_PIN in config:
        data_pin = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
        cg.add(var.set_pin_data(data_pin))
    else:
        await uart.register_uart_device(var, config)

    clock_pin = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_pin_clock(clock_pin))
    enable_pin = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
    cg.add(var.set_enable_pin(enable_pin))

    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_enable_count(config[CONF_ENABLE_COUNT]))

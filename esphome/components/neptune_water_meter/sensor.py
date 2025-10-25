import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_TIMEOUT, DEVICE_CLASS_WATER, STATE_CLASS_TOTAL_INCREASING

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
            cv.Optional(CONF_TIMEOUT, default=1000): cv.int_,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "neptune_water_meter",
    require_tx=False,
    require_rx=True,
    data_bits=7,
    parity="EVEN",
    stop_bits=2,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))

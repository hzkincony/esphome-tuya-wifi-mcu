import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID

from .. import TuyaWifiMcuComponent

DEPENDENCIES = ["tuya_wifi_mcu"]

CONF_TUYA_WIFI_MCU_ID = "tuya_wifi_mcu_id"
CONF_DP_ID = "dp_id"
CONF_BIND_BINARY_SENSOR_ID = "bind_binary_sensor_id"


tuya_wifi_mcu_ns = cg.esphome_ns.namespace("tuya_wifi_mcu")
TuyaWifiMcuBinarySensor = tuya_wifi_mcu_ns.class_(
    "TuyaWifiMcuBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(TuyaWifiMcuBinarySensor).extend(
    {
        cv.GenerateID(CONF_TUYA_WIFI_MCU_ID): cv.use_id(TuyaWifiMcuComponent),
        cv.Required(CONF_DP_ID): cv.int_range(min=1, max=255),
        cv.Optional(CONF_BIND_BINARY_SENSOR_ID): cv.use_id(
            binary_sensor.BinarySensor
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TUYA_WIFI_MCU_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_dp_id(config[CONF_DP_ID]))
    if CONF_BIND_BINARY_SENSOR_ID in config:
        bind_binary_sensor = await cg.get_variable(
            config[CONF_BIND_BINARY_SENSOR_ID]
        )
        cg.add(var.set_bind_binary_sensor(bind_binary_sensor))
    cg.add(parent.register_tuya_wifi_mcu_entity(var))

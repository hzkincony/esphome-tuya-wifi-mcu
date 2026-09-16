import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_OUTPUT, CONF_OUTPUT_ID

from .. import TuyaWifiMcuComponent

DEPENDENCIES = ["tuya_wifi_mcu"]

CONF_TUYA_WIFI_MCU_ID = "tuya_wifi_mcu_id"
CONF_DP_ID = "dp_id"
CONF_BIND_LIGHT_ID = "bind_light_id"


tuya_wifi_mcu_ns = cg.esphome_ns.namespace("tuya_wifi_mcu")
TuyaWifiMcuLightOutput = tuya_wifi_mcu_ns.class_(
    "TuyaWifiMcuLightOutput", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaWifiMcuLightOutput),
        cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
        cv.GenerateID(CONF_TUYA_WIFI_MCU_ID): cv.use_id(TuyaWifiMcuComponent),
        cv.Required(CONF_DP_ID): cv.int_,
        cv.Optional(CONF_BIND_LIGHT_ID): cv.use_id(light.LightState),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TUYA_WIFI_MCU_ID])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    out = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))

    if CONF_BIND_LIGHT_ID in config:
        bind_light = await cg.get_variable(config[CONF_BIND_LIGHT_ID])
        cg.add(var.set_bind_light(bind_light))

    cg.add(var.set_dp_id(config[CONF_DP_ID] & 0xFF))
    cg.add(parent.register_tuya_wifi_mcu_entity(var))

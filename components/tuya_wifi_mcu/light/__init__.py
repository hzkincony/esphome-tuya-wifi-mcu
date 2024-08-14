import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_ID, CONF_OUTPUT_ID, CONF_OUTPUT
from .. import TuyaWifiMcuComponent

DEPENDENCIES = ['tuya_wifi_mcu']

tuya_wifi_mcu_ns = cg.esphome_ns.namespace('tuya_wifi_mcu')
TuyaWifiMcuLightOutput = tuya_wifi_mcu_ns.class_(
    "TuyaWifiMcuLightOutput", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaWifiMcuLightOutput),
        cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
        cv.GenerateID("tuya_wifi_mcu_id"): cv.use_id(TuyaWifiMcuComponent),
        cv.Required("dp_id"): cv.int_,
        cv.Optional("bind_light_id"): cv.use_id(light)
    }
)

async def to_code(config):
    paren = await cg.get_variable(config["tuya_wifi_mcu_id"])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    cg.register_component(var, config)
    await light.register_light(var, config)

    out = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))

    if "bind_light_id" in config:
        bind_light = await cg.get_variable(config["bind_light_id"])
        cg.add(var.set_bind_light(bind_light))

    cg.add(var.set_dp_id(config["dp_id"]))
    cg.add(paren.register_tuya_wifi_mcu_entity(var))
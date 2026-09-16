import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .. import TuyaWifiMcuComponent

DEPENDENCIES = ["tuya_wifi_mcu"]

CONF_TUYA_WIFI_MCU_ID = "tuya_wifi_mcu_id"
CONF_DP_ID = "dp_id"
CONF_BIND_SWITCH_ID = "bind_switch_id"


tuya_wifi_mcu_ns = cg.esphome_ns.namespace("tuya_wifi_mcu")
TuyaWifiMcuSwitch = tuya_wifi_mcu_ns.class_(
    "TuyaWifiMcuSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = switch.switch_schema(TuyaWifiMcuSwitch).extend(
    {
        cv.GenerateID(CONF_TUYA_WIFI_MCU_ID): cv.use_id(TuyaWifiMcuComponent),
        cv.Required(CONF_DP_ID): cv.int_,
        cv.Optional(CONF_BIND_SWITCH_ID): cv.use_id(switch.Switch),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TUYA_WIFI_MCU_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    cg.add(var.set_dp_id(config[CONF_DP_ID] & 0xFF))
    if CONF_BIND_SWITCH_ID in config:
        bind_switch = await cg.get_variable(config[CONF_BIND_SWITCH_ID])
        cg.add(var.set_bind_switch(bind_switch))
    cg.add(parent.register_tuya_wifi_mcu_entity(var))

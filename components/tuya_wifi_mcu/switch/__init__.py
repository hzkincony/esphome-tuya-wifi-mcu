import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from .. import TuyaWifiMcuComponent

DEPENDENCIES = ['tuya_wifi_mcu']

tuya_wifi_mcu_ns = cg.esphome_ns.namespace('tuya_wifi_mcu')
tuyaWifiMcuSwitch = tuya_wifi_mcu_ns.class_('TuyaWifiMcuSwitch', switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(tuyaWifiMcuSwitch).extend({
    cv.GenerateID(): cv.declare_id(tuyaWifiMcuSwitch),
    cv.GenerateID("tuya_wifi_mcu_id"): cv.use_id(TuyaWifiMcuComponent),
    cv.Required("dp_id"): cv.int_,
    cv.Optional("bind_switch_id"): cv.use_id(switch)
}).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    paren = yield cg.get_variable(config["tuya_wifi_mcu_id"])
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    cg.add(var.set_dp_id(config["dp_id"]))
    if "bind_switch_id" in config:
        bind_switch = yield cg.get_variable(config["bind_switch_id"])
        cg.add(var.set_bind_switch(bind_switch))
    cg.add(paren.register_tuya_wifi_mcu_entity(var))

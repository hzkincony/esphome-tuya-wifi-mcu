import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from .. import TuyaWifiMcuComponent

DEPENDENCIES = ['tuya_wifi_mcu']

tuya_wifi_mcu_ns = cg.esphome_ns.namespace('tuya_wifi_mcu')
tuyaWifiMcuBinarySensor = tuya_wifi_mcu_ns.class_('TuyaWifiMcuBinarySensor', binary_sensor.BinarySensor, cg.Component)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(tuyaWifiMcuBinarySensor),
    cv.GenerateID("tuya_wifi_mcu_id"): cv.use_id(TuyaWifiMcuComponent),
    cv.Required("dp_id"): cv.int_,
    cv.Optional("bind_binary_sensor_id"): cv.use_id(binary_sensor)
}).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    paren = yield cg.get_variable(config["tuya_wifi_mcu_id"])
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)
    cg.add(var.set_dp_id(config["dp_id"]))
    if "bind_binary_sensor_id" in config:
        bind_binary_sensor = yield cg.get_variable(config["bind_binary_sensor_id"])
        cg.add(var.set_bind_binary_sensor(bind_binary_sensor))
    cg.add(paren.register_tuya_wifi_mcu_entity(var))

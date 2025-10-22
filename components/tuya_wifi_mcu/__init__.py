import esphome.codegen as cg
from esphome import automation
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_RX_PIN,
    CONF_TX_PIN,
)
from esphome.components import uart

DEPENDENCIES = ['uart']
MULTI_CONF = False

tuya_wifi_mcu_ns = cg.esphome_ns.namespace('tuya_wifi_mcu')

TuyaWifiMcuComponent = tuya_wifi_mcu_ns.class_('TuyaWifiMcuComponent', cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TuyaWifiMcuComponent),
    cv.Required("product_id"): cv.string,
    cv.Optional("mcu_verersion", default="1.0.0"): cv.string,
    cv.Optional("wifi_reset_pin", default=0): cv.int_range(min=0, max=99),
    cv.Optional("wifi_led_pin", default=0): cv.int_range(min=0, max=99),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

def to_code(config):
    cg.add_library("Tuya_WiFi_MCU_SDK", "66c750a8d136a766f4f0cedfc44ae6b1f1e9dffa", "https://github.com/idreamshen/tuya-wifi-mcu-sdk-arduino-library.git")
    u = yield cg.get_variable(config["uart_id"])
    var = cg.new_Pvariable(config[CONF_ID], u)
    cg.add(var.set_product_id(config["product_id"]))
    cg.add(var.set_version(config["mcu_verersion"]))
    cg.add(var.set_wifi_reset_pin(config["wifi_reset_pin"]))
    cg.add(var.set_wifi_led_pin(config["wifi_led_pin"]))
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = False

CONF_PRODUCT_ID = "product_id"
CONF_MCU_VERSION = "mcu_version"
CONF_LEGACY_MCU_VERSION = "mcu_verersion"
CONF_WIFI_CONTROL_MODE = "wifi_control_mode"
CONF_WIFI_RESET_PIN = "wifi_reset_pin"
CONF_WIFI_LED_PIN = "wifi_led_pin"


tuya_wifi_mcu_ns = cg.esphome_ns.namespace("tuya_wifi_mcu")
TuyaWifiMcuComponent = tuya_wifi_mcu_ns.class_(
    "TuyaWifiMcuComponent", cg.PollingComponent, uart.UARTDevice
)
WifiControlMode = tuya_wifi_mcu_ns.enum("WifiControlMode")

WIFI_CONTROL_MODES = {
    "mcu": WifiControlMode.WIFI_CONTROL_MODE_MCU,
    "module": WifiControlMode.WIFI_CONTROL_MODE_MODULE,
}


def _normalize_mcu_version(config):
    config = config.copy()
    if CONF_MCU_VERSION in config and CONF_LEGACY_MCU_VERSION in config:
        raise cv.Invalid(
            f"Specify only one of {CONF_MCU_VERSION} or {CONF_LEGACY_MCU_VERSION}"
        )
    if CONF_LEGACY_MCU_VERSION in config:
        config[CONF_MCU_VERSION] = config.pop(CONF_LEGACY_MCU_VERSION)
    config.setdefault(CONF_MCU_VERSION, "1.0.0")
    return config


COMMON_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TuyaWifiMcuComponent),
        cv.Required(CONF_PRODUCT_ID): cv.All(
            cv.string_strict, cv.Length(min=1, max=16)
        ),
        cv.Optional(CONF_MCU_VERSION): cv.All(
            cv.string_strict, cv.Length(min=1, max=5)
        ),
        cv.Optional(CONF_LEGACY_MCU_VERSION): cv.All(
            cv.string_strict, cv.Length(min=1, max=5)
        ),
    }
).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "mcu": COMMON_SCHEMA.extend(
                {
                    cv.Optional(CONF_WIFI_RESET_PIN): pins.gpio_input_pin_schema,
                    cv.Optional(CONF_WIFI_LED_PIN): pins.gpio_output_pin_schema,
                }
            ),
            "module": COMMON_SCHEMA.extend(
                {
                    cv.Required(CONF_WIFI_RESET_PIN): cv.int_range(min=0, max=255),
                    cv.Required(CONF_WIFI_LED_PIN): cv.int_range(min=0, max=255),
                }
            ),
        },
        key=CONF_WIFI_CONTROL_MODE,
        default_type="mcu",
        enum=WIFI_CONTROL_MODES,
        lower=True,
    ),
    _normalize_mcu_version,
)


async def to_code(config):
    uart_component = await cg.get_variable(config["uart_id"])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)

    cg.add(var.set_product_id(config[CONF_PRODUCT_ID]))
    cg.add(var.set_version(config[CONF_MCU_VERSION]))
    cg.add(var.set_wifi_control_mode(config[CONF_WIFI_CONTROL_MODE].enum_value))

    if config[CONF_WIFI_CONTROL_MODE] == "mcu":
        if CONF_WIFI_RESET_PIN in config:
            reset_pin = await cg.gpio_pin_expression(config[CONF_WIFI_RESET_PIN])
            cg.add(var.set_wifi_reset_pin(reset_pin))
        if CONF_WIFI_LED_PIN in config:
            led_pin = await cg.gpio_pin_expression(config[CONF_WIFI_LED_PIN])
            cg.add(var.set_wifi_led_pin(led_pin))
    else:
        cg.add(var.set_module_wifi_reset_pin(config[CONF_WIFI_RESET_PIN]))
        cg.add(var.set_module_wifi_led_pin(config[CONF_WIFI_LED_PIN]))

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

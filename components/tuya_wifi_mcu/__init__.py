import logging
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.const import (
    CONF_BUILD_FLAGS,
    CONF_ESPHOME,
    CONF_ID,
    CONF_INVERTED,
    CONF_NUMBER,
    CONF_PLATFORMIO_OPTIONS,
)
from esphome.core import CORE

DEPENDENCIES = ["uart"]
MULTI_CONF = False

CONF_PRODUCT_ID = "product_id"
CONF_MCU_VERSION = "mcu_version"
CONF_LEGACY_MCU_VERSION = "mcu_verersion"
CONF_WIFI_CONTROL_MODE = "wifi_control_mode"
CONF_WIFI_RESET_PIN = "wifi_reset_pin"
CONF_WIFI_LED_PIN = "wifi_led_pin"

_LOGGER = logging.getLogger(__name__)
_LEGACY_WIFI_CONTROL_FLAG = re.compile(
    r"(?<!\S)-D\s*WIFI_CONTROL_SELF_MODE(?:\s*=\s*([01]))?(?=\s|$)"
)


tuya_wifi_mcu_ns = cg.esphome_ns.namespace("tuya_wifi_mcu")
TuyaWifiMcuComponent = tuya_wifi_mcu_ns.class_(
    "TuyaWifiMcuComponent", cg.PollingComponent, uart.UARTDevice
)
WifiControlMode = tuya_wifi_mcu_ns.enum("WifiControlMode")

WIFI_CONTROL_MODES = {
    "mcu": WifiControlMode.WIFI_CONTROL_MODE_MCU,
    "module": WifiControlMode.WIFI_CONTROL_MODE_MODULE,
}


def _as_flag_strings(value):
    if isinstance(value, str):
        return [value]
    if isinstance(value, (list, tuple)):
        return [item for item in value if isinstance(item, str)]
    return []


def _legacy_wifi_control_mode():
    raw_config = getattr(CORE, "raw_config", None) or {}
    esphome_config = raw_config.get(CONF_ESPHOME) or {}
    values = _as_flag_strings(esphome_config.get(CONF_BUILD_FLAGS))

    platformio_options = esphome_config.get(CONF_PLATFORMIO_OPTIONS) or {}
    if isinstance(platformio_options, dict):
        for key, value in platformio_options.items():
            if key == CONF_BUILD_FLAGS or str(key).endswith(".extra_flags"):
                values.extend(_as_flag_strings(value))

    modes = set()
    for value in values:
        for match in _LEGACY_WIFI_CONTROL_FLAG.finditer(value):
            modes.add("module" if match.group(1) in (None, "1") else "mcu")

    if len(modes) > 1:
        raise cv.Invalid("Conflicting WIFI_CONTROL_SELF_MODE build flags")
    return next(iter(modes), None)


def _normalize_wifi_control_config(config):
    config = config.copy()
    legacy_mode = _legacy_wifi_control_mode()
    configured_mode = config.get(CONF_WIFI_CONTROL_MODE)
    if configured_mode is not None:
        configured_mode = cv.string_strict(configured_mode).lower()

    if legacy_mode is not None:
        _LOGGER.warning(
            "WIFI_CONTROL_SELF_MODE is deprecated; remove the build flag and use wifi_control_mode: %s",
            legacy_mode,
        )
        if configured_mode is not None and configured_mode != legacy_mode:
            raise cv.Invalid(
                "wifi_control_mode conflicts with the deprecated WIFI_CONTROL_SELF_MODE build flag"
            )
        config.setdefault(CONF_WIFI_CONTROL_MODE, legacy_mode)

    effective_mode = configured_mode or legacy_mode or "mcu"
    if effective_mode == "mcu":
        for key in (CONF_WIFI_RESET_PIN, CONF_WIFI_LED_PIN):
            value = config.get(key)
            try:
                number = cv.int_range(min=0, max=99)(value)
            except cv.Invalid:
                continue
            if number == 0:
                config.pop(key)
    return config


def _normalize_reset_pin(value):
    if isinstance(value, dict):
        value = value.copy()
        value.setdefault(CONF_INVERTED, True)
        return value
    return {CONF_NUMBER: value, CONF_INVERTED: True}


_reset_pin_schema = cv.All(_normalize_reset_pin, pins.gpio_input_pin_schema)


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
        cv.Required(CONF_PRODUCT_ID): cv.string,
        cv.Optional(CONF_MCU_VERSION): cv.string,
        cv.Optional(CONF_LEGACY_MCU_VERSION): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

CONFIG_SCHEMA = cv.All(
    _normalize_wifi_control_config,
    cv.typed_schema(
        {
            "mcu": COMMON_SCHEMA.extend(
                {
                    cv.Optional(CONF_WIFI_RESET_PIN): cv.Any(
                        cv.int_range(min=0, max=99), _reset_pin_schema
                    ),
                    cv.Optional(CONF_WIFI_LED_PIN): cv.Any(
                        cv.int_range(min=0, max=99), pins.gpio_output_pin_schema
                    ),
                }
            ),
            "module": COMMON_SCHEMA.extend(
                {
                    cv.Optional(CONF_WIFI_RESET_PIN, default=0): cv.int_range(min=0, max=255),
                    cv.Optional(CONF_WIFI_LED_PIN, default=0): cv.int_range(min=0, max=255),
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
            if isinstance(config[CONF_WIFI_RESET_PIN], int):
                cg.add(var.set_legacy_wifi_reset_pin(int(config[CONF_WIFI_RESET_PIN])))
            else:
                reset_pin = await cg.gpio_pin_expression(config[CONF_WIFI_RESET_PIN])
                cg.add(var.set_wifi_reset_pin(reset_pin))
        if CONF_WIFI_LED_PIN in config:
            if isinstance(config[CONF_WIFI_LED_PIN], int):
                cg.add(var.set_legacy_wifi_led_pin(int(config[CONF_WIFI_LED_PIN])))
            else:
                led_pin = await cg.gpio_pin_expression(config[CONF_WIFI_LED_PIN])
                cg.add(var.set_wifi_led_pin(led_pin))
    else:
        cg.add(var.set_module_wifi_reset_pin(config[CONF_WIFI_RESET_PIN]))
        cg.add(var.set_module_wifi_led_pin(config[CONF_WIFI_LED_PIN]))

    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import esphome.config_validation as cv
from esphome.core import CORE

from components.tuya_wifi_mcu import (
    CONF_WIFI_CONTROL_MODE,
    CONF_WIFI_LED_PIN,
    CONF_WIFI_RESET_PIN,
    _normalize_reset_pin,
    _normalize_wifi_control_config,
)


class LocatedInt(int):
    pass


class WifiControlConfigTest(unittest.TestCase):
    def setUp(self):
        self.had_raw_config = hasattr(CORE, "raw_config")
        self.previous_raw_config = getattr(CORE, "raw_config", None)

    def tearDown(self):
        if self.had_raw_config:
            CORE.raw_config = self.previous_raw_config
        else:
            delattr(CORE, "raw_config")

    def set_raw_esphome_config(self, config):
        CORE.raw_config = {"esphome": config}

    def test_legacy_mcu_flag_and_integer_zero_sentinels(self):
        self.set_raw_esphome_config(
            {"platformio_options": {"build_flags": "-DWIFI_CONTROL_SELF_MODE=0"}}
        )
        result = _normalize_wifi_control_config(
            {
                CONF_WIFI_RESET_PIN: LocatedInt(0),
                CONF_WIFI_LED_PIN: LocatedInt(0),
            }
        )
        self.assertEqual(result[CONF_WIFI_CONTROL_MODE], "mcu")
        self.assertNotIn(CONF_WIFI_RESET_PIN, result)
        self.assertNotIn(CONF_WIFI_LED_PIN, result)

    def test_legacy_module_flag_preserves_module_pin_zero(self):
        self.set_raw_esphome_config(
            {"platformio_options": {"build_flags": ["-DWIFI_CONTROL_SELF_MODE=1"]}}
        )
        result = _normalize_wifi_control_config(
            {CONF_WIFI_RESET_PIN: LocatedInt(0), CONF_WIFI_LED_PIN: LocatedInt(2)}
        )
        self.assertEqual(result[CONF_WIFI_CONTROL_MODE], "module")
        self.assertEqual(result[CONF_WIFI_RESET_PIN], 0)

    def test_explicit_gpio_zero_is_not_a_disable_sentinel(self):
        self.set_raw_esphome_config({})
        for pin in ("GPIO0", {"number": "GPIO0"}):
            with self.subTest(pin=pin):
                result = _normalize_wifi_control_config(
                    {CONF_WIFI_CONTROL_MODE: "mcu", CONF_WIFI_RESET_PIN: pin}
                )
                self.assertEqual(result[CONF_WIFI_RESET_PIN], pin)

    def test_boolean_false_is_not_a_disable_sentinel(self):
        self.set_raw_esphome_config({})
        result = _normalize_wifi_control_config(
            {CONF_WIFI_CONTROL_MODE: "mcu", CONF_WIFI_RESET_PIN: False}
        )
        self.assertIs(result[CONF_WIFI_RESET_PIN], False)

    def test_conflicting_legacy_and_yaml_modes_fail(self):
        self.set_raw_esphome_config({"build_flags": "-DWIFI_CONTROL_SELF_MODE=1"})
        with self.assertRaisesRegex(cv.Invalid, "conflicts"):
            _normalize_wifi_control_config({CONF_WIFI_CONTROL_MODE: "mcu"})

    def test_conflicting_legacy_flags_fail(self):
        self.set_raw_esphome_config(
            {
                "platformio_options": {
                    "board_build.extra_flags": (
                        "-DWIFI_CONTROL_SELF_MODE=0 -DWIFI_CONTROL_SELF_MODE=1"
                    )
                }
            }
        )
        with self.assertRaisesRegex(cv.Invalid, "Conflicting"):
            _normalize_wifi_control_config({})

    def test_matching_legacy_and_yaml_modes_are_allowed(self):
        self.set_raw_esphome_config(
            {"platformio_options": {"build_flags": "-DWIFI_CONTROL_SELF_MODE"}}
        )
        result = _normalize_wifi_control_config(
            {CONF_WIFI_CONTROL_MODE: "MODULE"}
        )
        self.assertEqual(result[CONF_WIFI_CONTROL_MODE], "MODULE")

    def test_reset_pin_defaults_to_active_low(self):
        self.assertEqual(
            _normalize_reset_pin(LocatedInt(5)), {"number": 5, "inverted": True}
        )
        self.assertEqual(
            _normalize_reset_pin({"number": "GPIO5"}),
            {"number": "GPIO5", "inverted": True},
        )

    def test_explicit_active_high_reset_pin_is_preserved(self):
        self.assertEqual(
            _normalize_reset_pin({"number": "GPIO5", "inverted": False}),
            {"number": "GPIO5", "inverted": False},
        )


if __name__ == "__main__":
    unittest.main()

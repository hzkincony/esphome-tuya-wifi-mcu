"""Configuration regressions against the unchanged main-branch example.

Run with: python -m unittest discover -s tests/config -p test_legacy_config.py -v
No firmware build, network access, or Tuya SDK is required.
"""

import asyncio
from contextlib import ExitStack
from copy import deepcopy
from pathlib import Path
import sys
import unittest
from unittest.mock import AsyncMock, MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import esphome.config as esphome_config
import esphome.config_validation as cv
from esphome import pins, yaml_util
from esphome.core import CORE

from components import tuya_wifi_mcu as tuya
from components.tuya_wifi_mcu import binary_sensor, light, switch


FIXTURE = Path(__file__).resolve().parents[1] / "compile" / "legacy-main.yaml"
PIN_KEYS = ("wifi_reset_pin", "wifi_led_pin")
ENTITY_MODULES = {"switch": switch, "binary_sensor": binary_sensor, "light": light}
DP_CASES = (-65537, -256, -1, 0, 1, 255, 256, 257, 65536, 2**64)


class LegacyConfigTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.addClassCleanup(CORE.reset)
        CORE.reset()
        CORE.config_path = FIXTURE
        result = esphome_config.read_config({}, skip_external_update=True)
        if result is None:
            raise AssertionError(
                f"The unchanged legacy fixture failed validation: {FIXTURE}; "
                "see the ESPHome diagnostics above"
            )
        cls.platform_data = deepcopy(CORE.data)
        CORE.reset()

    def setUp(self):
        self.reset_schema_context()

    def tearDown(self):
        CORE.reset()
        # CORE.reset also resets the pin registry in supported ESPHome versions.
        pins.PIN_SCHEMA_REGISTRY.reset()

    def reset_schema_context(self):
        CORE.reset()
        pins.PIN_SCHEMA_REGISTRY.reset()
        CORE.config_path = FIXTURE
        CORE.data = deepcopy(self.platform_data)
        CORE.raw_config = {"esphome": {}}

    def component_schema(self, **overrides):
        token = esphome_config.path_context.set(["tuya_wifi_mcu"])
        try:
            return tuya.CONFIG_SCHEMA(
                {"product_id": "legacy-product", "uart_id": "tuya_mcu_uart", **overrides}
            )
        finally:
            esphome_config.path_context.reset(token)

    def full_config(self, mutate=None):
        CORE.reset()
        pins.PIN_SCHEMA_REGISTRY.reset()
        CORE.config_path = FIXTURE
        raw = yaml_util.load_yaml(FIXTURE)
        if mutate is not None:
            mutate(raw)
        result = esphome_config.validate_config(raw, {}, skip_external_update=True)
        self.assertFalse(
            result.errors,
            "Full legacy configuration validation failed:\n"
            + "\n".join(str(error) for error in result.errors),
        )
        return result

    def test_original_main_fixture_read_config(self):
        CORE.reset()
        CORE.config_path = FIXTURE
        result = esphome_config.read_config({}, skip_external_update=True)
        self.assertIsNotNone(result, "See ESPHome validation diagnostics above")
        self.assertEqual(result["tuya_wifi_mcu"]["wifi_control_mode"], "mcu")
        self.assertEqual(result["tuya_wifi_mcu"]["mcu_version"], "1.0.0")
        self.assertEqual(result["tuya_wifi_mcu"]["wifi_reset_pin"], 5)
        self.assertEqual(result["tuya_wifi_mcu"]["wifi_led_pin"], 12)
        for domain in ENTITY_MODULES:
            self.assertTrue(
                any(entity["platform"] == "tuya_wifi_mcu" for entity in result[domain]),
                domain,
            )

    def test_product_id_keeps_legacy_string_coercion(self):
        for value in ("", "x", "x" * 1000, 123456, 12.5, "product-with-punctuation", "产品"):
            with self.subTest(product_id=value):
                self.reset_schema_context()
                result = self.component_schema(product_id=value)
                self.assertEqual(result["product_id"], cv.string(value))

    def test_misspelled_version_keeps_legacy_string_coercion(self):
        for value in ("", "1.0.0", "123.456.789", "v" * 1000, 123, 1.25):
            with self.subTest(mcu_verersion=value):
                self.reset_schema_context()
                result = self.component_schema(mcu_verersion=value)
                self.assertEqual(result["mcu_version"], cv.string(value))
                self.assertNotIn("mcu_verersion", result)

    def test_module_legacy_flag_defaults_both_pins_to_zero(self):
        CORE.raw_config = {
            "esphome": {
                "platformio_options": {
                    "board_build.extra_flags": ["-DWIFI_CONTROL_SELF_MODE=1"]
                }
            }
        }
        result = self.component_schema()
        self.assertEqual(result["wifi_control_mode"], "module")
        for key in PIN_KEYS:
            self.assertEqual(result[key], 0)

    def test_module_legacy_flag_without_pins_in_full_config(self):
        def mutate(raw):
            raw["esphome"]["platformio_options"]["board_build.extra_flags"] = [
                "-DWIFI_CONTROL_SELF_MODE=1"
            ]
            for key in PIN_KEYS:
                raw["tuya_wifi_mcu"].pop(key)

        result = self.full_config(mutate)["tuya_wifi_mcu"]
        self.assertEqual(result["wifi_control_mode"], "module")
        self.assertEqual([result[key] for key in PIN_KEYS], [0, 0])

    def test_mcu_all_legacy_zero_coercions_are_disabled(self):
        for key in PIN_KEYS:
            for value in (0, "0", "00", "0x0", 0.0, False):
                with self.subTest(pin=key, value=value, value_type=type(value).__name__):
                    self.reset_schema_context()
                    self.assertEqual(cv.int_range(min=0, max=99)(value), 0)
                    result = self.component_schema(**{key: value})
                    self.assertNotIn(key, result)

    def test_mcu_all_legacy_nonzero_pin_values_remain_scalars(self):
        values = (*range(1, 100), "5", "99", "0x05", "0x63", 5.0, 99.0, True)
        for key in PIN_KEYS:
            for value in values:
                with self.subTest(pin=key, value=value, value_type=type(value).__name__):
                    self.reset_schema_context()
                    expected = cv.int_range(min=0, max=99)(value)
                    result = self.component_schema(**{key: value})
                    self.assertIsInstance(result[key], int)
                    self.assertEqual(result[key], expected)
                    self.assertFalse(pins.PIN_SCHEMA_REGISTRY.pins_used)

    def test_explicit_gpio_zero_still_uses_gpio_schema(self):
        for key in PIN_KEYS:
            for value in ("GPIO0", {"number": "GPIO0"}):
                with self.subTest(pin=key, value=value):
                    self.reset_schema_context()
                    result = self.component_schema(**{key: value})
                    self.assertIsInstance(result[key], dict)
                    self.assertEqual(result[key]["number"], 0)
                    self.assertEqual(result[key]["inverted"], key == "wifi_reset_pin")
                    self.assertTrue(pins.PIN_SCHEMA_REGISTRY.pins_used)

    def test_explicit_gpio_dictionaries_still_reject_unavailable_pins(self):
        for key in PIN_KEYS:
            with self.subTest(pin=key):
                self.reset_schema_context()
                with self.assertRaises(cv.Invalid):
                    self.component_schema(**{key: {"number": 99}})

    def test_legacy_scalar_pin_reuse_passes_full_gpio_validation(self):
        for number in (5, 33):
            with self.subTest(pin=number):
                def mutate(raw):
                    # GPIO33 is already the UART TX pin in the original fixture.
                    for key in PIN_KEYS:
                        raw["tuya_wifi_mcu"][key] = number

                result = self.full_config(mutate)["tuya_wifi_mcu"]
                self.assertEqual([result[key] for key in PIN_KEYS], [number, number])

    def test_all_entity_schemas_keep_arbitrary_legacy_dp_integers(self):
        for domain, module in ENTITY_MODULES.items():
            for value in (*DP_CASES, "-1", "0x100", 3.0, False):
                with self.subTest(entity=domain, dp_id=value):
                    self.reset_schema_context()
                    config = {"name": "Legacy entity", "dp_id": value}
                    if domain == "light":
                        config["output"] = "legacy_output"
                    result = module.CONFIG_SCHEMA(config)
                    self.assertEqual(result["dp_id"], cv.int_(value))

    def test_all_entity_codegen_preserves_uint8_wire_ids(self):
        for domain, module in ENTITY_MODULES.items():
            for value in DP_CASES:
                with self.subTest(entity=domain, dp_id=value):
                    self.reset_schema_context()
                    config = {
                        "id": "legacy_entity",
                        "output_id": "legacy_light",
                        "output": "legacy_output",
                        "tuya_wifi_mcu_id": "legacy_parent",
                        "dp_id": value,
                    }
                    var = MagicMock()
                    with ExitStack() as stack:
                        stack.enter_context(
                            patch.object(module.cg, "get_variable", AsyncMock(return_value=MagicMock()))
                        )
                        stack.enter_context(patch.object(module.cg, "new_Pvariable", return_value=var))
                        stack.enter_context(patch.object(module.cg, "register_component", AsyncMock()))
                        stack.enter_context(patch.object(module.cg, "add"))
                        stack.enter_context(
                            patch.object(getattr(module, domain), f"register_{domain}", AsyncMock())
                        )
                        asyncio.run(module.to_code(config))
                    var.set_dp_id.assert_called_once_with(value & 0xFF)

    def test_wrapped_dp_ids_pass_full_config_for_all_entities(self):
        for value in (-1, 0, 256):
            with self.subTest(dp_id=value):
                def mutate(raw):
                    for domain in ENTITY_MODULES:
                        for entity in raw[domain]:
                            if entity["platform"] == "tuya_wifi_mcu":
                                entity["dp_id"] = value

                result = self.full_config(mutate)
                for domain in ENTITY_MODULES:
                    for entity in result[domain]:
                        if entity["platform"] == "tuya_wifi_mcu":
                            self.assertEqual(entity["dp_id"], value, domain)


if __name__ == "__main__":
    unittest.main(verbosity=2)

# ESPHome Tuya WiFi MCU

ESPHome external component for ESP32 devices that communicate with a Tuya WiFi MCU module over UART. The component supports both ESPHome frameworks:

- Arduino
- ESP-IDF

The Tuya serial protocol implementation is included in this component and no longer depends on the Arduino-only `Tuya_WiFi_MCU_SDK` library.

More KinCony hardware information: https://www.kincony.com

## Basic configuration

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf  # or arduino

external_components:
  - source:
      type: git
      url: https://github.com/hzkincony/esphome-tuya-wifi-mcu
      ref: main

uart:
  id: tuya_mcu_uart
  tx_pin: GPIO33
  rx_pin: GPIO14
  baud_rate: 9600

tuya_wifi_mcu:
  product_id: "abcdefghijklmnop"
  mcu_version: "1.0.0"
  uart_id: tuya_mcu_uart
  wifi_control_mode: mcu
  wifi_reset_pin:
    number: GPIO5
    mode: INPUT_PULLUP
  wifi_led_pin: GPIO12
```

## WiFi control modes

### `wifi_control_mode: mcu`

ESPHome handles the reset button and WiFi status LED. This is the default and is equivalent to the previous `WIFI_CONTROL_SELF_MODE=0` build flag.

- `wifi_reset_pin` is an optional ESPHome input pin. The button is active-low by default.
- `wifi_led_pin` is an optional ESPHome output pin.
- Integer GPIO shorthand remains supported.

### `wifi_control_mode: module`

The Tuya WiFi module handles its own reset button and status LED. This is equivalent to the previous `WIFI_CONTROL_SELF_MODE=1` build flag.

In this mode, `wifi_reset_pin` and `wifi_led_pin` are required numeric pin identifiers for the Tuya module. They are not ESP32 GPIOs.

```yaml
tuya_wifi_mcu:
  product_id: "abcdefghijklmnop"
  uart_id: tuya_mcu_uart
  wifi_control_mode: module
  wifi_reset_pin: 1
  wifi_led_pin: 2
```

## Entities

All Tuya entities require a one-byte `dp_id` in the range 1–255. Optional bind IDs synchronize the Tuya entity with another local ESPHome entity.

```yaml
switch:
  - platform: gpio
    id: relay_1
    pin: GPIO4

  - platform: tuya_wifi_mcu
    id: relay_1_tuya
    dp_id: 1
    bind_switch_id: relay_1
    internal: true

binary_sensor:
  - platform: template
    id: input_1
    lambda: return false;

  - platform: tuya_wifi_mcu
    id: input_1_tuya
    dp_id: 110
    bind_binary_sensor_id: input_1
    internal: true
```

Brightness lights use a Tuya `value` DP encoded as a four-byte big-endian value. This component currently maps brightness to the range 0–100.

```yaml
output:
  - platform: ledc
    id: dimmer_output
    pin: GPIO18

light:
  - platform: monochromatic
    id: local_dimmer
    output: dimmer_output

  - platform: tuya_wifi_mcu
    id: tuya_dimmer
    output: dimmer_output
    bind_light_id: local_dimmer
    dp_id: 173
    internal: true
```

## Migration from v1.x

1. Remove `WIFI_CONTROL_SELF_MODE` from `platformio_options` or other compiler flags.
2. Add `wifi_control_mode: mcu` for ESPHome-managed control, or `wifi_control_mode: module` for Tuya-module self-processing.
3. Rename the misspelled `mcu_verersion` option to `mcu_version`. The legacy spelling is still accepted for compatibility, but both spellings cannot be used together.
4. No external Tuya Arduino SDK is required.
5. Arduino and ESP-IDF use the same component and entity YAML.

## Verification

CI validates and compiles four combinations with ESPHome 2026.8.0:

- Arduino + MCU-managed WiFi controls
- Arduino + Tuya-module controls
- ESP-IDF + MCU-managed WiFi controls
- ESP-IDF + Tuya-module controls

The framework-independent protocol layer also has host tests for framing, checksums, fragmentation, malformed input, resynchronization, Boolean DP frame encoding, and four-byte value encoding.

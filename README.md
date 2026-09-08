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
  rx_buffer_size: 1024

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

- `wifi_reset_pin` is an optional ESPHome input pin. The button is active-low by default; set `inverted: false` for an active-high button.
- `wifi_led_pin` is an optional ESPHome output pin.
- Integer GPIO shorthand remains supported. For v1 compatibility, a bare integer `0` disables either optional pin. Use `GPIO0` or `{number: GPIO0}` when the ESP32 GPIO0 pin is intended.

### `wifi_control_mode: module`

The Tuya WiFi module handles its own reset button and status LED. This is equivalent to the previous `WIFI_CONTROL_SELF_MODE=1` build flag.

In this mode, `wifi_reset_pin` and `wifi_led_pin` are required numeric pin identifiers for the Tuya module. They are not ESP32 GPIOs, and numeric pin `0` remains a valid module pin.

```yaml
tuya_wifi_mcu:
  product_id: "abcdefghijklmnop"
  uart_id: tuya_mcu_uart
  wifi_control_mode: module
  wifi_reset_pin: 1
  wifi_led_pin: 2
```

## Entities

All Tuya entities require a one-byte `dp_id` in the range 1–255. Optional bind IDs synchronize the Tuya entity with another local ESPHome entity. Duplicate IDs remain supported for v1 compatibility: each downloaded record is sent to every entity with the same ID and DP type, with one acknowledgement per record.

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

1. Replace `WIFI_CONTROL_SELF_MODE=0` with `wifi_control_mode: mcu`, or `WIFI_CONTROL_SELF_MODE=1` with `wifi_control_mode: module`. The legacy compiler flag is detected temporarily and emits a deprecation warning; a conflicting flag and YAML mode fail validation.
2. In MCU mode, replace a bare integer pin `0` with no option when the pin should stay disabled. Explicit `GPIO0` remains a real ESP32 pin.
3. Rename the misspelled `mcu_verersion` option to `mcu_version`. The legacy spelling is still accepted for compatibility, but both spellings cannot be used together.
4. No external Tuya Arduino SDK is required.
5. Arduino and ESP-IDF use the same component and entity YAML.

The protocol parser has a bounded 1024-byte payload capacity. Set the UART `rx_buffer_size` to at least 1024 when the Tuya product may aggregate many DP records into one download frame.

## Verification

With Docker installed, the Makefile pins the official `ghcr.io/esphome/esphome:2026.8.0` container for reproducible local validation:

```bash
make host-test
make e2e-test
make config-test
make config-all
make compile-all
# Or run the complete matrix:
make ci
```

Override `ESPHOME_VERSION` or `ESPHOME_IMAGE` only when deliberately testing another ESPHome release. CI validates and compiles four combinations with the same pinned version:

- Arduino + MCU-managed WiFi controls
- Arduino + Tuya-module controls
- ESP-IDF + MCU-managed WiFi controls
- ESP-IDF + Tuya-module controls

The framework-independent `make host-test` tests cover framing, checksums, fragmentation, explicit idle timeout recovery, bounded oversized-frame skipping, duplicate-DP dispatch, brightness conversion, Boolean DP frame encoding, and four-byte value encoding.

### ESPHome host end-to-end tests

`make e2e-test` compiles `tests/e2e/host.yaml` with the real ESPHome `host` platform, then runs eight Python `unittest` scenarios. Each scenario starts a fresh native firmware process and connects it to a simulated Tuya module through a POSIX pseudo-terminal (PTY). An `aioesphomeapi` client controls and subscribes to the real ESPHome entities over the native API; no ESPHome runtime or component is mocked.

The scenarios cover:

- Heartbeat, product/work-mode queries, WiFi-state acknowledgement, and full DP state queries.
- Multi-DP downloads updating bound switches, binary sensors, lights, and observable output levels.
- Local switch/input changes uploading DPs, plus switch control through the Tuya entity.
- Local and Tuya light transitions reporting logical 0–100 brightness rather than gamma-corrected output or intermediate transition values.
- Repeated-download acknowledgements without duplicate/feedback-loop uploads.
- Bad checksums, unknown IDs, wrong DP types, invalid Boolean/brightness values, and malformed aggregate downloads.
- Fragmented UART frames, idle timeout recovery, and oversized frames containing embedded commands.

The Python runner and firmware execute in the same pinned Docker container, so no board, serial device mapping, Home Assistant instance, exposed port, or additional Python packages are required. The first build downloads the PlatformIO native platform; later builds reuse `.cache/` and `tests/e2e/.esphome/`. For an existing local Linux ESPHome environment with a C++ compiler, use `make e2e-test PYTHON=python3` instead.

The fixture uses MCU control mode without GPIOs, explicitly initializes both binary sensors, and suspends periodic polling to keep unsolicited reports out of assertions. Full-state reporting is exercised with protocol state queries. Initial-state callback semantics, physical GPIO behavior, the 60-second polling cadence, and the Tuya cloud are not covered by these host tests; the ESP32 compilation matrix remains separate.

Every run uses a temporary UART symlink, an available API port, and isolated preference storage. API/UART waits and compilation have timeouts, and firmware processes are terminated even when a test fails. Firmware logs are saved under `tests/e2e/.esphome/logs/`; failed tests also print their firmware log. CI runs `make e2e-test` as a separate job and uploads these logs on failure. Both `make test` and `make ci` include the e2e suite.

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

- Both pins are optional. The reset button is active-low by default.
- Legacy scalar pin values retain the original integer conversion and range 0–99. Nonzero values use the legacy GPIO path without ESPHome pin reservation or duplicate-use validation; existing shared-pin configurations remain accepted. This does not make every number a usable GPIO on every board.
- A scalar value that converts to zero disables the pin. This includes `0`, numeric strings such as `"0"` and `"0x0"`, `false`, and `0.0`.
- Explicit GPIO names and dictionaries use ESPHome's pin schemas and validation. Use `GPIO0` or `{number: GPIO0}` for the actual ESP32 GPIO0 pin, not the disabled-pin sentinel. A reset-pin dictionary defaults to `inverted: true`; set `inverted: false` for an active-high button. The LED uses an output-pin schema.

### `wifi_control_mode: module`

The Tuya WiFi module handles its own reset button and status LED. This is equivalent to the previous `WIFI_CONTROL_SELF_MODE=1` build flag.

In this mode, `wifi_reset_pin` and `wifi_led_pin` are optional numeric pin identifiers for the Tuya module, not ESP32 GPIOs. Each defaults to `0`, preserving the old behavior when both are omitted. Numeric pin `0` is a valid module pin, not a disable sentinel. The work-mode response sends the LED identifier first, followed by the reset identifier.

```yaml
tuya_wifi_mcu:
  product_id: "abcdefghijklmnop"
  uart_id: tuya_mcu_uart
  wifi_control_mode: module
  wifi_reset_pin: 1
  wifi_led_pin: 2
```

## Entities

All Tuya entities require an integer `dp_id`. As in the original component, any integer is accepted and its low eight bits are used on the wire: `-1` becomes `255`, `256` becomes `0`, and `257` becomes `1`. DP ID `0` is supported. Optional bind IDs synchronize the Tuya entity with another local ESPHome entity. Duplicate wire IDs remain supported: each downloaded record is sent to every entity with the same wire ID and DP type, with one acknowledgement per record.

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

Brightness lights use a Tuya `value` DP encoded as a four-byte big-endian value. Normal brightness maps to 0–100. Local brightness-to-percentage conversion truncates rather than rounds; remote integer percentages are retained exactly instead of being recomputed from floating-point light state. For compatibility, downloads above 100 are also accepted: physical brightness is clamped to `1.0`, while the stored and reported Tuya value retains the low eight bits of the download (`101` stays `101`, `256` becomes `0`, and `300` becomes `44`). A later local light command replaces that stored value with the local percentage. This preserves existing behavior; it is not a recommendation to send out-of-range brightness values.

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

## Compatibility with existing configurations

Existing component and entity YAML does not require migration to use Arduino or ESP-IDF:

- `WIFI_CONTROL_SELF_MODE=0` and `WIFI_CONTROL_SELF_MODE=1` remain supported and select MCU or module mode respectively. They emit a deprecation warning; switching to `wifi_control_mode` is optional. If both forms are specified, they must agree.
- Legacy scalar pins and zero-disable values remain accepted as described above. Omitting the pins in module mode preserves the old `0`/`0` defaults.
- The original misspelled `mcu_verersion` remains supported. `mcu_version` is an optional corrected spelling; use one spelling, not both. The default is still `"1.0.0"`.
- Product IDs and versions retain ESPHome's original string coercion, including numeric inputs and empty strings. There is no schema length or version-format restriction. Runtime storage safely truncates the product ID to 16 bytes and the version to 5 bytes; shorter values are stored as supplied. These are byte limits, not Unicode character limits, and should still match the Tuya product definition.
- No external Tuya Arduino SDK is required. Its uninitialized product-info storage and erroneous VALUE-download acknowledgement are not reproduced; compatibility does not require copying those SDK defects.

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

Override `ESPHOME_VERSION` or `ESPHOME_IMAGE` only when deliberately testing another ESPHome release. CI is configured to validate and compile five fixtures with the same pinned version:

- Arduino + MCU-managed WiFi controls
- Arduino + Tuya-module controls
- ESP-IDF + MCU-managed WiFi controls
- ESP-IDF + Tuya-module controls
- `legacy-main`: the original main-branch Arduino example, unchanged except for the local external-component path

`make config-test` uses standard-library `unittest` discovery to run 22 tests across both configuration test files. The legacy regressions load the original fixture through ESPHome's real configuration reader, check old scalar and string coercions, exercise complete configurations with shared pins and wrapped DP IDs, and verify all three entity code generators use the low eight DP-ID bits. They also distinguish the permissive legacy scalar pin `99` from an explicit GPIO dictionary with `number: 99`, which still fails ESPHome GPIO validation. These Python configuration tests require neither a firmware build nor the old SDK.

The framework-independent `make host-test` tests cover framing, checksums, fragmentation, explicit idle timeout recovery, bounded oversized-frame skipping, duplicate-DP dispatch, brightness conversion, Boolean DP frame encoding, and four-byte value encoding.

### ESPHome host end-to-end tests

`make e2e-test` compiles three fixtures with the real ESPHome `host` platform and is configured to run 28 Python `unittest` cases:

- `tests/e2e/host.yaml`: 13 scenarios in `HostE2ETest`.
- `tests/e2e/host-legacy.yaml`: the same 13 inherited scenarios in `LegacyHostE2ETest`.
- `tests/e2e/host-filtered.yaml`: two binary-sensor filter scenarios in `FilteredHostE2ETest`.

All three use the shared `HostFirmwareTest` harness. Each case starts a fresh native firmware process and connects it to a simulated Tuya module through a POSIX pseudo-terminal (PTY). An `aioesphomeapi` client controls and subscribes to the real ESPHome entities over the native API; no ESPHome runtime or component is mocked.

The legacy fixture reuses the base fixture through `packages` and exercises the original configuration forms end to end:

- `WIFI_CONTROL_SELF_MODE=1` selects module mode, with omitted pins reported as LED `0`, reset `0`.
- Numeric `product_id: 123456789012345678901` is converted to a string and truncated to `"1234567890123456"` on the wire.
- The legacy `mcu_verersion: "1.2.34567"` spelling produces the five-byte version `"1.2.3"`.
- Switch DP `256`, binary-sensor DP `-1`, and light DP `259` use wire IDs `0`, `255`, and `3` respectively.

The base and legacy fixtures run the same scenarios covering:

- Heartbeat, product/work-mode queries, WiFi-state acknowledgement, and full DP state queries.
- Separate first-download `false` and `true` cases starting with both binary sensors uninitialized. Each verifies the Tuya sensor and its bound sensor acquire the requested state and produce exactly one acknowledgement.
- Multi-DP downloads updating bound switches, binary sensors, lights, and observable output levels.
- Local switch/input changes uploading DPs, plus switch control through the Tuya entity.
- Local and Tuya light transitions reporting logical brightness rather than gamma-corrected output or intermediate transition values. Percentage conversion truncates rather than rounds: `0.429` reports `42`, and `0.005` reports `0`.
- Every integer brightness download from `0` through `100` retaining its exact reported value, including `53` and `59`; floating-point light-state updates must not turn these into `52` and `58`.
- Legacy brightness downloads `101`, `255`, `256`, `300`, and `UINT32_MAX`: the stored and reported state retains the low byte while the output is clamped to `1.0`. Subsequent local 100% commands through either the Tuya light or its bound light restore a reported state of `100`.
- Repeated-download acknowledgements without duplicate/feedback-loop uploads.
- Bad checksums, unknown IDs, wrong DP types, invalid Boolean values, and malformed aggregate downloads.
- Fragmented UART frames, idle timeout recovery, and oversized frames containing embedded commands.

The filtered fixture also reuses the base through `packages`, adding `delayed_on: 1s` to `tuya_input`. Its two scenarios verify that a first `true` download delays the first publication on both the Tuya sensor and its bound sensor, and that later `true` downloads are delayed and can be cancelled by a following `false` download. Accepted Boolean downloads receive an immediate acknowledgement echoing the input, preserving the old SDK's valid behavior; this acknowledgement is not a claim that a filter has already published the value. State queries report the actual post-filter state, and a delayed publication generates a separate state report. Binary-sensor synchronization uses full-state callbacks so the first valid publication is propagated without bypassing filters.

The Python runner and firmware execute in the same pinned Docker container, so no board, serial device mapping, Home Assistant instance, exposed port, or additional Python packages are required. The first build downloads the PlatformIO native platform; later builds reuse `.cache/` and `tests/e2e/.esphome/`. For an existing local Linux ESPHome environment with a C++ compiler, use `make e2e-test PYTHON=python3` instead.

The base and filtered fixtures use MCU control mode without GPIOs; the legacy fixture uses module mode with default pin identifiers. None of the fixtures preinitializes either binary sensor. Ordinary test setup initializes them with a first UART `false` download, while the dedicated first-download cases start from missing state. Periodic polling is suspended to keep unsolicited reports out of assertions; full-state reporting is exercised with protocol state queries. Physical GPIO behavior, the 60-second polling cadence, and the Tuya cloud are not covered by these host tests. The ESP32 compilation matrix remains separate and is not a substitute for hardware testing.

Every run uses a temporary UART symlink, an available API port, and isolated preference storage. API/UART waits and compilation have timeouts, and firmware processes are terminated even when a test fails. Firmware logs are saved under `tests/e2e/.esphome/logs/`; failed tests also print their firmware log. CI runs `make e2e-test` as a separate job and uploads these logs on failure. Both `make test` and `make ci` include the e2e suite.

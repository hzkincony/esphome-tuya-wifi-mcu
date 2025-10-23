# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESPHome external component that enables ESP32 devices to communicate with Tuya WiFi MCU modules over UART. It allows ESPHome devices to integrate with the Tuya IoT ecosystem while maintaining local control through Home Assistant.

The component acts as a bridge between ESPHome entities (switches, binary sensors, lights) and Tuya data points (DPs), synchronizing state bidirectionally.

## Architecture

### Core Component Structure

- **TuyaWifiMcuComponent** (`tuya_wifi_mcu_component.h/cpp`): Main component that manages UART communication with the Tuya WiFi module via the Tuya WiFi MCU SDK. It implements:
  - Static singleton pattern for callback registration with the Tuya SDK
  - Entity registration system to track all Tuya-enabled entities
  - DP (Data Point) processing and state synchronization
  - WiFi LED and reset button handling (when `WIFI_CONTROL_SELF_MODE=0`)

- **TuyaWifiMcuEntity** (`tuya_wifi_mcu_entity.h`): Abstract base class for all Tuya-enabled entities. Subclasses must implement:
  - `get_dp_type()`: Returns DP type (DP_TYPE_BOOL, DP_TYPE_VALUE, etc.)
  - `report_tuya_dp_state()`: Reports current state to Tuya cloud
  - `process_dp_data()`: Handles incoming DP data from Tuya

### Entity Types

Each entity type has both Python (ESPHome config validation) and C++ (runtime behavior) implementations:

1. **Switch** (`switch/`):
   - Supports binding to another ESPHome switch to sync states
   - Uses DP_TYPE_BOOL
   - Typical use: relay outputs synced with Tuya app

2. **Binary Sensor** (`binary_sensor/`):
   - Read-only state reporting to Tuya
   - Supports binding to another ESPHome binary sensor
   - Typical use: input state monitoring

3. **Light** (`light/`):
   - Brightness-only light support
   - Uses DP_TYPE_VALUE for brightness (0-255)
   - Requires binding to a FloatOutput component
   - Supports binding to another ESPHome light to sync states

### State Synchronization

The component maintains bidirectional state sync:
- **ESPHome → Tuya**: `report_tuya_dp_state()` called periodically (60s polling) or when state changes
- **Tuya → ESPHome**: `dp_process()` receives incoming DP updates and calls `process_dp_data()` on matching entities
- **Bind mechanism**: When entities are bound (e.g., `bind_switch_id`), they mirror the state of the local ESPHome entity

### WiFi Control Modes

Controlled by `WIFI_CONTROL_SELF_MODE` platformio flag:
- `0`: ESPHome manages WiFi LED and reset button
- `1`: Tuya MCU SDK manages WiFi LED and reset button

## Configuration

### Required YAML Structure

```yaml
tuya_wifi_mcu:
  product_id: "xxxxxx"          # Tuya product ID
  uart_id: tuya_mcu_uart        # UART component ID
  wifi_reset_pin: 5             # GPIO for WiFi reset button
  wifi_led_pin: 12              # GPIO for WiFi status LED
```

### Entity Configuration Pattern

All Tuya entities require:
- `dp_id`: Tuya data point ID (must match Tuya product definition)
- `tuya_wifi_mcu_id`: Reference to parent component (auto-injected)
- Optional `bind_*_id`: Link to another ESPHome entity for state mirroring

Typically, entities are marked `internal: true` to hide from Home Assistant UI, with state synced via bound entities.

## Development Commands

This is an ESPHome external component library, not a standalone application. There are no build or test commands for this repository itself.

### Testing Changes

To test modifications:
1. Reference the local component in an ESPHome device configuration:
   ```yaml
   external_components:
     - source:
         type: local
         path: /path/to/esphome-tuya-wifi-mcu/components
   ```
2. Build and flash the ESPHome device using standard ESPHome CLI or dashboard

### Release Process

Releases are automated via GitHub Actions:
- Create a GitHub release with a version tag (e.g., `v1.1.0`)
- Workflow automatically zips the `components/` directory and attaches to release
- Users reference releases via git URL in `external_components`

## Dependencies

- **Tuya WiFi MCU SDK**: Auto-installed via platformio library manager
  - Library: "Tuya_WiFi_MCU_SDK"
  - Git commit: `66c750a8d136a766f4f0cedfc44ae6b1f1e9dffa`
  - Repository: https://github.com/idreamshen/tuya-wifi-mcu-sdk-arduino-library.git

## Code Patterns

### Adding New Entity Types

1. Create subdirectory under `components/tuya_wifi_mcu/`
2. Implement `__init__.py` for ESPHome config schema (extends base entity schema, requires `dp_id`)
3. Implement `.h` and `.cpp` files inheriting from `TuyaWifiMcuEntity`
4. Override: `get_dp_type()`, `report_tuya_dp_state()`, `process_dp_data()`
5. Register with parent component: `cg.add(paren.register_tuya_wifi_mcu_entity(var))`

### Static Callback Pattern

The Tuya SDK requires static C callbacks. The component uses a singleton pattern:
- `static TuyaWifiMcuComponent* instance` stores the single component instance
- `static_dp_process()` and `static_dp_update()` forward to instance methods
- Registered in `setup()` via `tuya_wifi_->dp_process_func_register(static_dp_process)`

## Hardware Context

Designed for KinCony ESP32 boards with Tuya WiFi MCU modules connected via UART. Common use cases include multi-relay boards (e.g., KC868-E16T) and DAC-based dimmer boards (e.g., KC868-A32 Pro).
- /root/projects/github/esphome read-only
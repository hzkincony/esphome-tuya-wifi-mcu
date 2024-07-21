# esphome-tuya-wifi-mcu
more information, you can check with KinCony's webpage: https://www.kincony.com

```yaml
esphome:
  name: example_device_name
  friendly_name: example_device_name
  platformio_options:
    board_build.extra_flags:
      # WIFI_CONTROL_SELF_MODE = 0
      # WIFI_CONTROL_SELF_MODE = 1
      - "-DWIFI_CONTROL_SELF_MODE=0"

external_components:
  - source:
      type: git
      url: https://github.com/hzkincony/esphome-tuya-wifi-mcu
      ref: v1.1.0

uart:
  tx_pin: 33
  rx_pin: 14
  id: tuya_mcu_uart
  baud_rate: 9600

tuya_wifi_mcu:
  # tuya mcu product id
  product_id: xxxxxx 
  uart_id: tuya_mcu_uart
  wifi_reset_pin: 5
  wifi_led_pin: 12

i2c:
  sda: 16
  scl: 15
  scan: true
  id: bus_a

pcf8574:
  - id: 'pcf8574_hub_out_1'  # for output channel 1-8
    address: 0x21

switch:
  - platform: gpio
    name: "e16t-output1"
    id: "e16t_output1"
    pin:
      pcf8574: pcf8574_hub_out_1
      number: 0
      mode: OUTPUT
      inverted: true
  - platform: tuya_wifi_mcu
    name: e16t-output1-tuya
    dp_id: 1
    # hide from homeassistant ui
    internal: true
    # bind other switch, sync state
    bind_switch_id: "e16t_output1"
  - platform: gpio
    name: "e16t-output2"
    id: "e16t_output2"
    pin:
      pcf8574: pcf8574_hub_out_1
      number: 1
      mode: OUTPUT
      inverted: true
  - platform: tuya_wifi_mcu
    name: e16t-output2-tuya
    dp_id: 2
    internal: true
    bind_switch_id: "e16t_output2"

binary_sensor:
  - platform: template
    name: "e16t-binary1"
    id: e16t_binary1
    lambda: |-
      if (id(e16t_output1).state) {
        return true;
      } else {
        return false;
      }
  - platform: tuya_wifi_mcu
    name: e16t-binary-tuya
    dp_id: 110
    bind_binary_sensor_id: e16t_binary1
    internal: true
```

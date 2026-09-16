#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include "tuya_protocol.h"
#include "tuya_wifi_mcu_entity.h"

namespace esphome {
namespace tuya_wifi_mcu {

static const char *const TAG = "tuya.wifi.mcu";

enum WifiControlMode : uint8_t {
  WIFI_CONTROL_MODE_MCU = 0,
  WIFI_CONTROL_MODE_MODULE = 1,
};

class TuyaWifiMcuComponent : public PollingComponent,
                            public uart::UARTDevice,
                            public TuyaProtocolListener {
 public:
  explicit TuyaWifiMcuComponent(uart::UARTComponent *uart_component)
      : PollingComponent(60000), UARTDevice(uart_component), protocol_(this) {}

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;
  float get_setup_priority() const override;

  void set_product_id(const std::string &product_id) { this->product_id_ = product_id.substr(0, 16); }
  void set_version(const std::string &mcu_version) { this->mcu_version_ = mcu_version.substr(0, 5); }
  void set_wifi_control_mode(WifiControlMode mode) { this->wifi_control_mode_ = mode; }
  void set_wifi_reset_pin(GPIOPin *wifi_reset_pin) { this->wifi_reset_pin_ = wifi_reset_pin; }
  void set_wifi_led_pin(GPIOPin *wifi_led_pin) { this->wifi_led_pin_ = wifi_led_pin; }
  void set_legacy_wifi_reset_pin(uint8_t pin) { this->legacy_wifi_reset_pin_ = pin; }
  void set_legacy_wifi_led_pin(uint8_t pin) { this->legacy_wifi_led_pin_ = pin; }
  void set_module_wifi_reset_pin(uint8_t wifi_reset_pin) { this->module_wifi_reset_pin_ = wifi_reset_pin; }
  void set_module_wifi_led_pin(uint8_t wifi_led_pin) { this->module_wifi_led_pin_ = wifi_led_pin; }

  void reset_tuya_wifi();
  void report_tuya_dp_states();
  void report_bool_dp(uint8_t dp_id, bool value);
  void report_value_dp(uint8_t dp_id, uint32_t value);

  void register_tuya_wifi_mcu_entity(TuyaWifiMcuEntity *entity) {
    entity->set_parent(this);
    this->entities_.push_back(entity);
  }

  void on_tuya_frame(const TuyaFrame &frame) override;

 protected:
  static constexpr uint16_t MAX_TX_PAYLOAD_SIZE = 64;

  void send_frame_(TuyaCommand command, const uint8_t *payload = nullptr, uint16_t payload_length = 0);
  void process_dp_download_(const uint8_t *payload, uint16_t payload_length);
  bool validate_dp_payload_(const uint8_t *payload, uint16_t payload_length) const;
  void handle_reset_button_(uint32_t now);
  void update_wifi_led_(uint32_t now);

  TuyaProtocolParser protocol_;
  std::string product_id_;
  std::string mcu_version_{"1.0.0"};
  WifiControlMode wifi_control_mode_{WIFI_CONTROL_MODE_MCU};
  GPIOPin *wifi_reset_pin_{nullptr};
  GPIOPin *wifi_led_pin_{nullptr};
  uint8_t legacy_wifi_reset_pin_{0};
  uint8_t legacy_wifi_led_pin_{0};
  uint8_t module_wifi_reset_pin_{0};
  uint8_t module_wifi_led_pin_{0};
  uint8_t wifi_work_state_{0xFF};
  bool first_heartbeat_{true};
  bool wifi_led_state_{false};
  uint32_t last_wifi_led_state_change_time_{0};
  bool reset_raw_pressed_{false};
  bool reset_stable_pressed_{false};
  bool reset_press_handled_{false};
  uint32_t reset_transition_time_{0};
  std::vector<TuyaWifiMcuEntity *> entities_;
  std::array<uint8_t, MAX_TX_PAYLOAD_SIZE + 7> tx_buffer_{};
};

}  // namespace tuya_wifi_mcu
}  // namespace esphome

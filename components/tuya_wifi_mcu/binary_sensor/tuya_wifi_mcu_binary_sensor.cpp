#include "tuya_wifi_mcu_binary_sensor.h"

#include "../tuya_wifi_mcu_component.h"

namespace esphome {
namespace tuya_wifi_mcu {

void TuyaWifiMcuBinarySensor::setup() {
  this->add_full_state_callback([this](optional<bool>, optional<bool> current) {
    if (this->syncing_ || !current.has_value()) {
      return;
    }

    const bool state = current.value();
    this->syncing_ = true;
    if (this->bind_binary_sensor_ != nullptr) {
      this->bind_binary_sensor_->publish_state(state);
    }
    this->syncing_ = false;

    if (!this->is_processing_remote()) {
      this->report_tuya_dp_state();
    }
  });

  if (this->bind_binary_sensor_ != nullptr) {
    this->bind_binary_sensor_->add_full_state_callback([this](optional<bool>, optional<bool> current) {
      if (this->syncing_ || !current.has_value()) {
        return;
      }

      const bool state = current.value();
      this->syncing_ = true;
      this->publish_state(state);
      this->syncing_ = false;

      if (!this->is_processing_remote()) {
        this->report_tuya_dp_state();
      }
    });
  }
}

bool TuyaWifiMcuBinarySensor::process_dp_data(const uint8_t *value, uint16_t length) {
  if (length != 1 || value[0] > 1) {
    return false;
  }

  // Forward every input so filters can cancel a pending transition.
  this->publish_state(value[0] != 0);
  return true;
}

void TuyaWifiMcuBinarySensor::report_tuya_dp_state() {
  if (this->parent_ != nullptr) {
    this->parent_->report_bool_dp(this->get_dp_id(), this->state);
  }
}

void TuyaWifiMcuBinarySensor::acknowledge_tuya_dp(const uint8_t *value, uint16_t) {
  // A filter may delay publication, but the download acknowledgement echoes the accepted input.
  if (this->parent_ != nullptr) {
    this->parent_->report_bool_dp(this->get_dp_id(), value[0] != 0);
  }
}

void TuyaWifiMcuBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya WiFi MCU binary sensor DP %u", this->get_dp_id());
}

}  // namespace tuya_wifi_mcu
}  // namespace esphome

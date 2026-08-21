#include "tuya_wifi_mcu_switch.h"

#include "../tuya_wifi_mcu_component.h"

namespace esphome {
namespace tuya_wifi_mcu {

void TuyaWifiMcuSwitch::setup() {
  this->add_on_state_callback([this](bool state) {
    if (this->syncing_) {
      return;
    }

    this->syncing_ = true;
    if (this->bind_switch_ != nullptr && this->bind_switch_->state != state) {
      if (state) {
        this->bind_switch_->turn_on();
      } else {
        this->bind_switch_->turn_off();
      }
    }
    this->syncing_ = false;

    if (!this->is_processing_remote()) {
      this->report_tuya_dp_state();
    }
  });

  if (this->bind_switch_ != nullptr) {
    this->bind_switch_->add_on_state_callback([this](bool state) {
      if (this->syncing_ || this->state == state) {
        return;
      }

      this->syncing_ = true;
      if (state) {
        this->turn_on();
      } else {
        this->turn_off();
      }
      this->syncing_ = false;

      if (!this->is_processing_remote()) {
        this->report_tuya_dp_state();
      }
    });
  }
}

bool TuyaWifiMcuSwitch::process_dp_data(const uint8_t *value, uint16_t length) {
  if (length != 1 || value[0] > 1) {
    return false;
  }

  const bool state = value[0] != 0;
  if (this->state != state) {
    if (state) {
      this->turn_on();
    } else {
      this->turn_off();
    }
  }
  return true;
}

void TuyaWifiMcuSwitch::report_tuya_dp_state() {
  if (this->parent_ != nullptr) {
    this->parent_->report_bool_dp(this->get_dp_id(), this->state);
  }
}

void TuyaWifiMcuSwitch::dump_config() { ESP_LOGCONFIG(TAG, "Tuya WiFi MCU switch DP %u", this->get_dp_id()); }

void TuyaWifiMcuSwitch::write_state(bool state) { this->publish_state(state); }

}  // namespace tuya_wifi_mcu
}  // namespace esphome

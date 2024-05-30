#include "tuya_wifi_mcu_binary_sensor.h"

namespace esphome {
  namespace tuya_wifi_mcu {
    void TuyaWifiMcuBinarySensor::setup() {
      ESP_LOGD(TAG, "TuyaWifiMcuBinarySensor::setup");

      this->add_on_state_callback([this](bool state) {
        ESP_LOGD(TAG, "state_callback, state=%d", state ? 1 : 0);
        unsigned char tuya_dp_state = state ? 1 : 0;
        if (this->tuya_wifi_ == nullptr) {
          return;
        }
        this->tuya_wifi_->mcu_dp_update(dp_id_, tuya_dp_state, 1);
        ESP_LOGD(TAG, "updated dp=%d state=%d", dp_id_, tuya_dp_state);

        if (this->is_bind_) {
          this->bind_binary_sensor_->publish_state(state);
        }
      });

      if (this->is_bind_) {
        this->bind_binary_sensor_->add_on_state_callback([this](bool state) {
          ESP_LOGD(TAG, "bind_binary_sensor state_callback, state=%d", state ? 1 : 0);
          this->publish_state(state);
        });
      }
      
    }

    void TuyaWifiMcuBinarySensor::process_dp_data(const unsigned char value[], unsigned short length) {
      if (this->tuya_wifi_ == nullptr) {
        return;
      }
      auto state = this->tuya_wifi_->mcu_get_dp_download_data(this->dp_id_, value, length);
      if (state == 1) {
        this->publish_state(true);
      } else if (state == 0) {
        this->publish_state(false);
      } else {
        // wrong state
      }
    }

    void TuyaWifiMcuBinarySensor::report_tuya_dp_state() {
      if (this->tuya_wifi_ == nullptr) {
        return;
      }
      unsigned char tuya_dp_state = this->state ? 1 : 0;
      this->tuya_wifi_->mcu_dp_update(this->get_dp_id(), tuya_dp_state, 1);
      ESP_LOGD(TAG, "updated dp=%d state=%d", this->get_dp_id(), tuya_dp_state);
    }

    void TuyaWifiMcuBinarySensor::dump_config(){
      ESP_LOGCONFIG(TAG, "TuyaWifiMcuBinarySensor::dump_config");
    }
    
  }
}
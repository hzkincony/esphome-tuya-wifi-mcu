#include "tuya_wifi_mcu_switch.h"

namespace esphome {
  namespace tuya_wifi_mcu {
    void TuyaWifiMcuSwitch::setup() {
      ESP_LOGD(TAG, "TuyaWifiMcuSwitch::setup");

      this->add_on_state_callback([this](bool state) {
        ESP_LOGD(TAG, "state_callback, state=%d", state ? 1 : 0);
        unsigned char tuya_dp_state = state ? 1 : 0;
        if (this->tuya_wifi_ == nullptr) {
          return;
        }
        this->tuya_wifi_->mcu_dp_update(dp_id_, tuya_dp_state, 1);
        ESP_LOGD(TAG, "updated dp=%d state=%d", dp_id_, tuya_dp_state);

        if (this->is_bind_) {
          if (state) {
            this->bind_switch_->turn_on();
          } else {
            this->bind_switch_->turn_off();
          }
        }
      });

      if (this->is_bind_) {
        this->bind_switch_->add_on_state_callback([this](bool state) {
          ESP_LOGD(TAG, "bind_switch state_callback, state=%d", state ? 1 : 0);
          if (state) {
            this->turn_on();
          } else {
            this->turn_off();
          }
        });
      }
      
    }

    void TuyaWifiMcuSwitch::process_dp_data(const unsigned char value[], unsigned short length) {
      if (this->tuya_wifi_ == nullptr) {
        return;
      }
      auto state = this->tuya_wifi_->mcu_get_dp_download_data(this->dp_id_, value, length);
      if (state == 1) {
        this->turn_on();
      } else if (state == 0) {
        this->turn_off();
      } else {
        // wrong state
      }
    }

    void TuyaWifiMcuSwitch::report_tuya_dp_state() {
      if (this->tuya_wifi_ == nullptr) {
        return;
      }
      unsigned char tuya_dp_state = this->state ? 1 : 0;
      this->tuya_wifi_->mcu_dp_update(this->get_dp_id(), tuya_dp_state, 1);
      ESP_LOGD(TAG, "updated dp=%d state=%d", this->get_dp_id(), tuya_dp_state);
    }

    void TuyaWifiMcuSwitch::dump_config(){
      ESP_LOGCONFIG(TAG, "TuyaWifiMcuSwitch::dump_config");
    }

    void TuyaWifiMcuSwitch::write_state(bool state) {
      this->publish_state(state);
    }

    
  }
}
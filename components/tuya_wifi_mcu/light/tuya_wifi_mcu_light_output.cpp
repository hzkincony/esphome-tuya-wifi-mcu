#include "tuya_wifi_mcu_light_output.h"
#include "esphome/core/helpers.h"

namespace esphome {
  namespace tuya_wifi_mcu {
    void TuyaWifiMcuLightOutput::setup() {
      ESP_LOGD(TAG, "TuyaWifiMcuLightOutput::setup");

      if (this->is_bind_) {
        this->bind_light_->add_new_remote_values_callback([this]() {
          ESP_LOGD(TAG, "callback");

          float bright;
          this->bind_light_->current_values_as_brightness(&bright);
          this->tuya_bright_ = gamma_uncorrect(bright, this->bind_light_->get_gamma_correct()) * 100;

          if (this->tuya_wifi_ == nullptr) {
            return;
          }
          this->tuya_wifi_->mcu_dp_update(this->get_dp_id(), this->tuya_bright_, 1);
          ESP_LOGD(TAG, "updated dp=%d state=%d", this->get_dp_id(), this->tuya_bright_);
        });
      }
    }

    void TuyaWifiMcuLightOutput::process_dp_data(const unsigned char value[], unsigned short length) {
      auto state = this->tuya_wifi_->mcu_get_dp_download_data(this->dp_id_, value, length);
      this->tuya_bright_ = state;
      float bright = state / 100.0;
      ESP_LOGD(TAG, "process data, dp=%d bright=%f", this->dp_id_, bright);

      if (this->is_bind_) {
        this->output_->set_level(gamma_correct(bright, this->bind_light_->get_gamma_correct()));
      } else {
        this->output_->set_level(gamma_correct(bright, 2.8));
      }
    }

    void TuyaWifiMcuLightOutput::write_state(light::LightState *state) {
      
    }

    void TuyaWifiMcuLightOutput::report_tuya_dp_state() {
      if (this->tuya_wifi_ == nullptr) {
        return;
      }
      this->tuya_wifi_->mcu_dp_update(this->get_dp_id(), this->tuya_bright_, 1);
      ESP_LOGD(TAG, "updated dp=%d state=%d", this->get_dp_id(), this->tuya_bright_);
    }

    void TuyaWifiMcuLightOutput::dump_config(){
      ESP_LOGCONFIG(TAG, "TuyaWifiMcuLightOutput::dump_config");
    }
    
  }
}
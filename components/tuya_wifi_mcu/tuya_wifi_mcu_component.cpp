#include "tuya_wifi_mcu_component.h"
#include "esphome/components/network/util.h"
#include <string>

namespace esphome {
  namespace tuya_wifi_mcu {

    TuyaWifiMcuComponent* TuyaWifiMcuComponent::instance = nullptr;

    void TuyaWifiMcuComponent::setup() {
      ESP_LOGD(TAG, "TuyaWifiMcuComponent::setup");
      
      if (this->wifi_led_pin_ != 0) {
        pinMode(this->wifi_led_pin_, OUTPUT);
        digitalWrite(this->wifi_led_pin_, LOW);
      }

      if (this->wifi_reset_pin_ != 0) {
        pinMode(this->wifi_reset_pin_, INPUT);
      }

      tuya_wifi_ = new TuyaWifi(static_cast<esphome::uart::ESP32ArduinoUARTComponent*>(this->uart_)->get_hw_serial());

      // set_tuya_wifi for sensors
      for(auto &entity : this->entities_) {
        entity->set_tuya_wifi(tuya_wifi_);
      }

      tuya_wifi_->init(
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(product_id_)),
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(mcu_version_)));

      for(auto &entity : this->entities_) {
        dps_.push_back({entity->get_dp_id(), entity->get_dp_type()});
      }
      
      unsigned char (*dps_array)[2] = new unsigned char[dps_.size()][2];

      for (size_t i = 0; i < dps_.size(); ++i) {
          dps_array[i][0] = dps_[i][0];
          dps_array[i][1] = dps_[i][1];
      }

      tuya_wifi_->set_dp_cmd_total(dps_array, dps_.size());
      tuya_wifi_->dp_process_func_register(static_dp_process);
      tuya_wifi_->dp_update_all_func_register(static_dp_update);
    }

    void TuyaWifiMcuComponent::dump_config() {

    }

    void TuyaWifiMcuComponent::loop() {
      tuya_wifi_->uart_service();

      if (wifi_reset_pin_ != 0) {
        if (digitalRead(wifi_reset_pin_) == LOW) {
          delay(20);
          if (digitalRead(wifi_reset_pin_) == LOW) {
            this->reset_tuya_wifi();
          }
        }
      }

      if (this->wifi_led_pin_ != 0) {
        if (tuya_wifi_->mcu_get_wifi_work_state() == WIFI_CONN_CLOUD) {
          if (this->wifi_led_state_ == LOW) {
            this->wifi_led_state_ = HIGH;
            digitalWrite(wifi_led_pin_, this->wifi_led_state_);
          }
        } else if ((tuya_wifi_->mcu_get_wifi_work_state() != WIFI_LOW_POWER) && (tuya_wifi_->mcu_get_wifi_work_state() != WIFI_CONN_CLOUD) && (tuya_wifi_->mcu_get_wifi_work_state() != WIFI_SATE_UNKNOW)) {
          if (millis()- this->last_wifi_led_state_change_time_ >= 500) {
            this->last_wifi_led_state_change_time_ = millis();

            if (this->wifi_led_state_ == LOW) {
              this->wifi_led_state_ = HIGH;
            } else {
              this->wifi_led_state_ = LOW;
            }
            digitalWrite(wifi_led_pin_, this->wifi_led_state_);
          }
        }
      }

      

      delay(10);
    }

    void TuyaWifiMcuComponent::reset_tuya_wifi() {
      ESP_LOGD(TAG, "begin smart config");
      tuya_wifi_->mcu_set_wifi_mode(SMART_CONFIG);
    }

    void TuyaWifiMcuComponent::update() {
      // ESP_LOGD(TAG, "TuyaIotComponent::update, wifi_reset_pin=%d, network=%d, switches_.size=%d", this->wifi_reset_pin_, network::is_connected(), switches_.size());
      ESP_LOGD(TAG, "update");
      this->report_tuya_dp_states();
    }

    unsigned char TuyaWifiMcuComponent::static_dp_process(unsigned char dpid, const unsigned char value[], unsigned short length) {
      return instance->dp_process(dpid, value, length);
    }

    unsigned char TuyaWifiMcuComponent::dp_process(unsigned char dpid, const unsigned char value[], unsigned short length) {
      ESP_LOGD(TAG, "dp_process");

      for(auto &entity : this->entities_) {
        if (entity->get_dp_id() == dpid) {
          entity->process_dp_data(value, length);
        }
      }

      this->tuya_wifi_->mcu_dp_update(dpid, value, length);
      return TY_SUCCESS;
    }

    void TuyaWifiMcuComponent::static_dp_update() {
      instance->dp_update();
    }

    void TuyaWifiMcuComponent::report_tuya_dp_states() {
      for (auto &entity : this->entities_) {
        entity->report_tuya_dp_state();
      }
    }

    void TuyaWifiMcuComponent::dp_update() {
      ESP_LOGD(TAG, "dp_update");
      this->report_tuya_dp_states();
    }

    float TuyaWifiMcuComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

  }

  
}

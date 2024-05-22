#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include "../tuya_wifi_mcu_entity.h"

#include <TuyaWifi.h>

namespace esphome {
  namespace tuya_wifi_mcu {
    class TuyaWifiMcuBinarySensor : public TuyaWifiMcuEntity, public Component, public binary_sensor::BinarySensor {
    public:
      void set_dp_id(uint8_t dp_id) { this->dp_id_ = dp_id; };
      void set_bind_binary_sensor(binary_sensor::BinarySensor* bind_binary_sensor) { 
        this->is_bind_ = true;
        this->bind_binary_sensor_ = bind_binary_sensor;
      };

      uint8_t get_dp_type() { return DP_TYPE_BOOL; };

      void setup() override;
      void dump_config() override;
      void process_dp_data(const unsigned char value[], unsigned short length) override;
      void report_tuya_dp_state() override;

    protected:
      bool is_bind_ = false;
      binary_sensor::BinarySensor* bind_binary_sensor_;
    };
  }
}
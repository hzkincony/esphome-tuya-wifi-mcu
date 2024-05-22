#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

#include "../tuya_wifi_mcu_entity.h"

#include <TuyaWifi.h>

namespace esphome {
  namespace tuya_wifi_mcu {
    class TuyaWifiMcuSwitch : public TuyaWifiMcuEntity, public Component, public switch_::Switch {
    public:
      void set_dp_id(uint8_t dp_id) { this->dp_id_ = dp_id; };
      void set_bind_switch(switch_::Switch* switch_) { 
        this->is_bind_ = true;
        this->bind_switch_ = switch_;
      };

      uint8_t get_dp_type() { return DP_TYPE_BOOL; };

      void setup() override;
      void write_state(bool state) override;
      void dump_config() override;
      void process_dp_data(const unsigned char value[], unsigned short length) override;
      void report_tuya_dp_state() override;

    protected:
      //TuyaWifi* tuya_wifi_;
      bool is_bind_ = false;
      switch_::Switch* bind_switch_;
      //uint8_t dp_id_;
    };
  }
}
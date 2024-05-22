#pragma once

#include <TuyaWifi.h>

namespace esphome {
  namespace tuya_wifi_mcu {
    class TuyaWifiMcuEntity {
        public:
            void set_tuya_wifi(TuyaWifi* tuya_wifi) { this->tuya_wifi_ = tuya_wifi;};
            uint8_t get_dp_id() { return this->dp_id_; };

            virtual uint8_t get_dp_type()=0;
            virtual void report_tuya_dp_state() {};
            virtual void process_dp_data(const unsigned char value[], unsigned short length) {};
        protected:
            TuyaWifi* tuya_wifi_;
            uint8_t dp_id_;
    };
  }
}
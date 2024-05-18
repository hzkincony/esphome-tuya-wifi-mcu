#pragma once

#include "esphome.h"

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"

#include <TuyaWifi.h>

#define TAG "tuya.wifi.mcu"

namespace esphome {
  namespace tuya_wifi_mcu {

    class TuyaWifiMcuSwitch : public Component, public switch_::Switch {
    public:
      void set_dp_id(uint8_t dp_id) { this->dp_id_ = dp_id; };
      uint8_t get_dp_id() { return this->dp_id_; };
      void set_tuya_wifi_(TuyaWifi* tuya_wifi) { this->tuya_wifi_ = tuya_wifi;};
      void set_bind_switch(switch_::Switch* switch_) { 
        this->is_bind_ = true;
        this->bind_switch_ = switch_;
      };

      void setup() override;
      void write_state(bool state) override;
      void dump_config() override;

    protected:
      TuyaWifi* tuya_wifi_;
      bool is_bind_ = false;
      switch_::Switch* bind_switch_;
      uint8_t dp_id_;
    };

    class TuyaWifiMcuComponent : public PollingComponent, public uart::UARTDevice {
    public:
      TuyaWifiMcuComponent(uart::UARTComponent *uartComponent) : PollingComponent(60000), UARTDevice(uartComponent) {
        this->uart_ = uartComponent;
        instance = this;
      }

      void setup() override;
      void dump_config() override;
      void update() override;
      void loop() override;
      float get_setup_priority() const override;

      void set_product_id(const char* product_id) { this-> product_id_ = product_id; };
      void set_version(const char* mcu_version) { this-> mcu_version_ = mcu_version; };
      void set_wifi_reset_pin(uint8_t wifi_reset_pin) { this->wifi_reset_pin_ = wifi_reset_pin; };
      void set_wifi_led_pin(uint8_t wifi_led_pin) { this->wifi_led_pin_ = wifi_led_pin; };

      void reset_tuya_wifi();
      void report_tuya_dp_states();
      unsigned char dp_process(unsigned char dpid, const unsigned char value[], unsigned short length);
      void dp_update();
      uart::UARTComponent* get_uart() {
        return this->uart_;
      }

      void register_switch(tuya_wifi_mcu::TuyaWifiMcuSwitch *obj)  {
        this->switches_.push_back(obj);
      };

    protected:
      TuyaWifi* tuya_wifi_;
      uart::UARTComponent* uart_;
      uint8_t wifi_reset_pin_;
      uint8_t wifi_led_pin_;
      uint8_t wifi_led_state_ = 0;
      unsigned long last_wifi_led_state_change_time_ = 0;
      const char* product_id_;
      const char* mcu_version_;
      std::vector<std::array<unsigned char, 2>> dps_;
      std::vector<tuya_wifi_mcu::TuyaWifiMcuSwitch *> switches_;
    
    private:
      static TuyaWifiMcuComponent* instance;
      static unsigned char static_dp_process(unsigned char dpid, const unsigned char value[], unsigned short length);
      static void static_dp_update();
    };

    

  }
}

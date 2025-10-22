#pragma once

#include "esphome.h"

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "tuya_wifi_mcu_entity.h"

namespace esphome {
  namespace tuya_wifi_mcu {

    static const char *const TAG = "tuya.wifi.mcu"; 

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
      void set_uart_num(uint8_t uart_num) { this->uart_num_ = uart_num; };
      void set_uart_tx_pin(int8_t uart_tx_pin) { this->uart_tx_pin_ = uart_tx_pin; };
      void set_uart_rx_pin(int8_t uart_rx_pin) { this->uart_rx_pin_ = uart_rx_pin; };
      void set_uart_baud_rate(uint32_t uart_baud_rate) { this->uart_baud_rate_ = uart_baud_rate; };

      void reset_tuya_wifi();
      void report_tuya_dp_states();
      unsigned char dp_process(unsigned char dpid, const unsigned char value[], unsigned short length);
      void dp_update();
      uart::UARTComponent* get_uart() {
        return this->uart_;
      }

      void register_tuya_wifi_mcu_entity(tuya_wifi_mcu::TuyaWifiMcuEntity *obj)  {
        this->entities_.push_back(obj);
      };

    protected:
      TuyaWifi* tuya_wifi_;
      uart::UARTComponent* uart_;
      uint8_t uart_num_{1};
      int8_t uart_tx_pin_{-1};
      int8_t uart_rx_pin_{-1};
      uint32_t uart_baud_rate_{9600};
      uint8_t wifi_reset_pin_;
      uint8_t wifi_led_pin_;
      uint8_t wifi_led_state_{0};
      unsigned long last_wifi_led_state_change_time_{0};
      const char* product_id_;
      const char* mcu_version_;
      std::vector<std::array<unsigned char, 2>> dps_;
      std::vector<tuya_wifi_mcu::TuyaWifiMcuEntity *> entities_;
    
    private:
      static TuyaWifiMcuComponent* instance;
      static unsigned char static_dp_process(unsigned char dpid, const unsigned char value[], unsigned short length);
      static void static_dp_update();
    };

    

  }
}

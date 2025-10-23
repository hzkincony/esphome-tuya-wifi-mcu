#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace tuya_wifi_mcu {

/**
 * A wrapper class that provides HardwareSerial-compatible interface
 * but delegates to ESPHome's UARTComponent internally.
 * This allows the Tuya SDK to work with ESPHome's UART without conflicts.
 */
class UARTWrapper : public HardwareSerial {
 public:
  // Initialize HardwareSerial with a dummy UART number (we won't actually use it)
  UARTWrapper(uart::UARTComponent *uart) : HardwareSerial(255), uart_(uart) {}

  // Stream interface implementation
  int available() { return uart_->available(); }

  int peek() {
    uint8_t data;
    if (uart_->peek_byte(&data)) {
      return data;
    }
    return -1;
  }

  int read() {
    uint8_t data;
    if (uart_->read_byte(&data)) {
      return data;
    }
    return -1;
  }

  size_t write(uint8_t c) {
    uart_->write_byte(c);
    return 1;
  }

  size_t write(const uint8_t *buffer, size_t size) {
    uart_->write_array(buffer, size);
    return size;
  }

  void flush() { uart_->flush(); }

  // HardwareSerial-like methods
  void begin(unsigned long baud, uint32_t config = 0, int8_t rxPin = -1, int8_t txPin = -1,
             bool invert = false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112) {
    // ESPHome UART is already initialized, do nothing
  }

  void end() {
    // Don't end ESPHome UART
  }

  operator bool() const { return true; }

 protected:
  uart::UARTComponent *uart_;
};

}  // namespace tuya_wifi_mcu
}  // namespace esphome

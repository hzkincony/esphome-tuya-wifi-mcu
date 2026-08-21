#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

#include "../tuya_wifi_mcu_entity.h"

namespace esphome {
namespace tuya_wifi_mcu {

class TuyaWifiMcuBinarySensor : public TuyaWifiMcuEntity, public Component, public binary_sensor::BinarySensor {
 public:
  void set_bind_binary_sensor(binary_sensor::BinarySensor *bind_binary_sensor) {
    this->bind_binary_sensor_ = bind_binary_sensor;
  }

  TuyaDpType get_dp_type() const override { return TuyaDpType::BOOLEAN; }

  void setup() override;
  void dump_config() override;
  bool process_dp_data(const uint8_t *value, uint16_t length) override;
  void report_tuya_dp_state() override;

 protected:
  binary_sensor::BinarySensor *bind_binary_sensor_{nullptr};
  bool syncing_{false};
};

}  // namespace tuya_wifi_mcu
}  // namespace esphome

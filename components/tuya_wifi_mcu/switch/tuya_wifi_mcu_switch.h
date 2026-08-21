#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include "../tuya_wifi_mcu_entity.h"

namespace esphome {
namespace tuya_wifi_mcu {

class TuyaWifiMcuSwitch : public TuyaWifiMcuEntity, public Component, public switch_::Switch {
 public:
  void set_bind_switch(switch_::Switch *bind_switch) { this->bind_switch_ = bind_switch; }

  TuyaDpType get_dp_type() const override { return TuyaDpType::BOOLEAN; }

  void setup() override;
  void write_state(bool state) override;
  void dump_config() override;
  bool process_dp_data(const uint8_t *value, uint16_t length) override;
  void report_tuya_dp_state() override;

 protected:
  switch_::Switch *bind_switch_{nullptr};
  bool syncing_{false};
};

}  // namespace tuya_wifi_mcu
}  // namespace esphome

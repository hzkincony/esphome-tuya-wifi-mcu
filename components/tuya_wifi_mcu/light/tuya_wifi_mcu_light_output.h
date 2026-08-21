#pragma once

#include <cstdint>

#include "esphome/components/light/light_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"

#include "../tuya_wifi_mcu_entity.h"

namespace esphome {
namespace tuya_wifi_mcu {

class TuyaWifiMcuLightOutput : public TuyaWifiMcuEntity,
                              public Component,
                              public light::LightOutput,
                              public light::LightRemoteValuesListener {
 public:
  void set_bind_light(light::LightState *light) { this->bind_light_ = light; }
  void set_output(output::FloatOutput *output) { this->output_ = output; }

  TuyaDpType get_dp_type() const override { return TuyaDpType::VALUE; }

  void setup() override;
  void setup_state(light::LightState *state) override { this->own_state_ = state; }
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
  void on_light_remote_values_update() override;
  void dump_config() override;
  bool process_dp_data(const uint8_t *value, uint16_t length) override;
  void report_tuya_dp_state() override;

 protected:
  light::LightState *bind_light_{nullptr};
  light::LightState *own_state_{nullptr};
  output::FloatOutput *output_{nullptr};
  uint32_t tuya_brightness_{0};
  bool syncing_{false};
};

}  // namespace tuya_wifi_mcu
}  // namespace esphome

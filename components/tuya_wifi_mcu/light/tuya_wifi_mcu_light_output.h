#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"

#include "../tuya_wifi_mcu_entity.h"

namespace esphome {
  namespace tuya_wifi_mcu {
    class TuyaWifiMcuLightOutput : public TuyaWifiMcuEntity, public Component, public light::LightOutput {
    public:
      void set_dp_id(uint8_t dp_id) { this->dp_id_ = dp_id; };
      void set_bind_light(light::LightState* light) { 
        this->is_bind_ = true;
        this->bind_light_ = light;
      };

      uint8_t get_dp_type() { return DP_TYPE_VALUE; };

      void setup() override;
      void set_output(output::FloatOutput *output) { output_ = output; }
      light::LightTraits get_traits() override {
        auto traits = light::LightTraits();
        traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
        return traits;
      }
      void write_state(light::LightState *state);
      void dump_config() override;
      void process_dp_data(const unsigned char value[], unsigned short length) override;
      void report_tuya_dp_state() override;

    protected:
      bool is_bind_{false};
      light::LightState* bind_light_;
      float state_{0};
      output::FloatOutput *output_;
      uint8_t tuya_bright_;
    };
  }
}
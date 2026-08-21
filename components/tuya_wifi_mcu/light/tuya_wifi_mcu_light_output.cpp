#include "tuya_wifi_mcu_light_output.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/helpers.h"

#include "../tuya_wifi_mcu_component.h"

namespace esphome {
namespace tuya_wifi_mcu {

void TuyaWifiMcuLightOutput::setup() {
  if (this->output_ == nullptr) {
    ESP_LOGE(TAG, "Tuya WiFi MCU light requires an output");
    this->mark_failed();
    return;
  }

  if (this->bind_light_ != nullptr) {
    this->bind_light_->add_remote_values_listener(this);
  }
}

void TuyaWifiMcuLightOutput::on_light_remote_values_update() {
  if (this->syncing_) {
    return;
  }

  float brightness;
  this->bind_light_->current_values_as_brightness(&brightness);
  const float linear = std::max(0.0f, std::min(1.0f, this->bind_light_->gamma_uncorrect_lut(brightness)));
  const uint32_t tuya_brightness = static_cast<uint32_t>(std::lround(linear * 100.0f));
  if (this->tuya_brightness_ == tuya_brightness) {
    return;
  }
  this->tuya_brightness_ = tuya_brightness;
  if (!this->is_processing_remote()) {
    this->report_tuya_dp_state();
  }
}

light::LightTraits TuyaWifiMcuLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void TuyaWifiMcuLightOutput::write_state(light::LightState *state) {
  this->own_state_ = state;
  float brightness;
  state->current_values_as_brightness(&brightness);
  const float linear = std::max(0.0f, std::min(1.0f, state->gamma_uncorrect_lut(brightness)));
  const uint32_t tuya_brightness = static_cast<uint32_t>(std::lround(linear * 100.0f));

  this->output_->set_level(brightness);
  if (this->tuya_brightness_ != tuya_brightness) {
    this->tuya_brightness_ = tuya_brightness;
    if (!this->is_processing_remote()) {
      this->report_tuya_dp_state();
    }
  }
}

bool TuyaWifiMcuLightOutput::process_dp_data(const uint8_t *value, uint16_t length) {
  if (length != 4) {
    return false;
  }

  const uint32_t tuya_brightness = decode_tuya_value(value);
  if (tuya_brightness > 100) {
    ESP_LOGW(TAG, "Ignoring out-of-range brightness %u for DP %u", static_cast<unsigned>(tuya_brightness),
             this->get_dp_id());
    return false;
  }

  this->tuya_brightness_ = tuya_brightness;
  const float brightness = static_cast<float>(tuya_brightness) / 100.0f;
  const float output_level = this->bind_light_ != nullptr
                                 ? this->bind_light_->gamma_correct_lut(brightness)
                                 : (this->own_state_ != nullptr ? this->own_state_->gamma_correct_lut(brightness)
                                                                : brightness);
  this->syncing_ = true;
  this->output_->set_level(output_level);
  this->syncing_ = false;
  return true;
}

void TuyaWifiMcuLightOutput::report_tuya_dp_state() {
  if (this->parent_ != nullptr) {
    this->parent_->report_value_dp(this->get_dp_id(), this->tuya_brightness_);
  }
}

void TuyaWifiMcuLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya WiFi MCU brightness light DP %u", this->get_dp_id());
}

}  // namespace tuya_wifi_mcu
}  // namespace esphome

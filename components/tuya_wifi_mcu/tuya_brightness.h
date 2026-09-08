#pragma once

#include <algorithm>
#include <cstdint>

namespace esphome {
namespace tuya_wifi_mcu {

inline uint32_t tuya_brightness_from_linear(float brightness) {
  const float clamped = std::max(0.0f, std::min(1.0f, brightness));
  return static_cast<uint32_t>(clamped * 100.0f);
}

}  // namespace tuya_wifi_mcu
}  // namespace esphome

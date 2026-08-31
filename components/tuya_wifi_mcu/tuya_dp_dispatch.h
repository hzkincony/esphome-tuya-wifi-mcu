#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tuya_wifi_mcu_entity.h"

namespace esphome {
namespace tuya_wifi_mcu {

struct TuyaDpDispatchResult {
  size_t id_matches{0};
  size_t type_matches{0};
  size_t accepted{0};
};

inline TuyaDpDispatchResult dispatch_tuya_dp(const std::vector<TuyaWifiMcuEntity *> &entities, uint8_t dp_id,
                                             TuyaDpType type, const uint8_t *value, uint16_t length) {
  TuyaDpDispatchResult result;
  TuyaWifiMcuEntity *ack_entity = nullptr;

  for (auto *entity : entities) {
    if (entity != nullptr && entity->get_dp_id() == dp_id) {
      entity->set_processing_remote(true);
      result.id_matches++;
    }
  }

  for (auto *entity : entities) {
    if (entity == nullptr || entity->get_dp_id() != dp_id || entity->get_dp_type() != type) {
      continue;
    }
    result.type_matches++;
    if (entity->process_dp_data(value, length)) {
      result.accepted++;
      if (ack_entity == nullptr) {
        ack_entity = entity;
      }
    }
  }

  for (auto *entity : entities) {
    if (entity != nullptr && entity->get_dp_id() == dp_id) {
      entity->set_processing_remote(false);
    }
  }

  if (ack_entity != nullptr) {
    ack_entity->report_tuya_dp_state();
  }
  return result;
}

}  // namespace tuya_wifi_mcu
}  // namespace esphome

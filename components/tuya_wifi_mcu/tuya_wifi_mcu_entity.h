#pragma once

#include <cstdint>

#include "tuya_protocol.h"

namespace esphome {
namespace tuya_wifi_mcu {

class TuyaWifiMcuComponent;

class TuyaWifiMcuEntity {
 public:
  void set_parent(TuyaWifiMcuComponent *parent) { this->parent_ = parent; }
  void set_dp_id(uint8_t dp_id) { this->dp_id_ = dp_id; }
  uint8_t get_dp_id() const { return this->dp_id_; }

  void set_processing_remote(bool processing_remote) { this->processing_remote_ = processing_remote; }
  bool is_processing_remote() const { return this->processing_remote_; }

  virtual TuyaDpType get_dp_type() const = 0;
  virtual void report_tuya_dp_state() = 0;
  virtual void acknowledge_tuya_dp(const uint8_t *, uint16_t) { this->report_tuya_dp_state(); }
  virtual bool process_dp_data(const uint8_t *value, uint16_t length) = 0;

 protected:
  TuyaWifiMcuComponent *parent_{nullptr};
  uint8_t dp_id_{0};
  bool processing_remote_{false};
};

}  // namespace tuya_wifi_mcu
}  // namespace esphome

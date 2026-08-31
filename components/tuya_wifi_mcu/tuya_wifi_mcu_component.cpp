#include "tuya_wifi_mcu_component.h"

#include <algorithm>
#include <cstdio>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "tuya_dp_dispatch.h"

namespace esphome {
namespace tuya_wifi_mcu {

static constexpr uint8_t WIFI_CONN_CLOUD = 0x04;
static constexpr uint8_t WIFI_LOW_POWER = 0x05;
static constexpr uint8_t SMART_CONFIG = 0x00;
static constexpr uint32_t RESET_DEBOUNCE_MS = 20;
static constexpr uint32_t WIFI_LED_BLINK_INTERVAL_MS = 500;
static constexpr size_t UART_READ_BATCH_SIZE = 64;

void TuyaWifiMcuComponent::setup() {
  ESP_LOGD(TAG, "Setting up Tuya WiFi MCU component");

  for (auto *entity : this->entities_) {
    if (entity == nullptr || entity->get_dp_id() == 0) {
      ESP_LOGE(TAG, "Invalid Tuya entity registration");
      this->mark_failed();
      return;
    }
  }

  if (this->wifi_control_mode_ == WIFI_CONTROL_MODE_MCU) {
    if (this->wifi_led_pin_ != nullptr) {
      this->wifi_led_pin_->setup();
      this->wifi_led_pin_->digital_write(false);
    }
    if (this->wifi_reset_pin_ != nullptr) {
      this->wifi_reset_pin_->setup();
    }
  }
}

void TuyaWifiMcuComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya WiFi MCU:");
  ESP_LOGCONFIG(TAG, "  Product ID: %s", this->product_id_.c_str());
  ESP_LOGCONFIG(TAG, "  MCU version: %s", this->mcu_version_.c_str());
  ESP_LOGCONFIG(TAG, "  WiFi control mode: %s",
                this->wifi_control_mode_ == WIFI_CONTROL_MODE_MCU ? "MCU" : "module");
  ESP_LOGCONFIG(TAG, "  Registered DPs: %u", static_cast<unsigned>(this->entities_.size()));
  if (this->wifi_control_mode_ == WIFI_CONTROL_MODE_MCU) {
    LOG_PIN("  WiFi reset pin: ", this->wifi_reset_pin_);
    LOG_PIN("  WiFi LED pin: ", this->wifi_led_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Module WiFi reset pin: %u", this->module_wifi_reset_pin_);
    ESP_LOGCONFIG(TAG, "  Module WiFi LED pin: %u", this->module_wifi_led_pin_);
  }
}

void TuyaWifiMcuComponent::loop() {
  std::array<uint8_t, UART_READ_BATCH_SIZE> buffer{};
  bool uart_idle = false;
  while (true) {
    const size_t available = this->available();
    if (available == 0) {
      uart_idle = true;
      break;
    }

    const size_t length = std::min(available, buffer.size());
    if (!this->read_array(buffer.data(), length)) {
      break;
    }

    const uint32_t read_time = millis();
    for (size_t i = 0; i < length; i++) {
      this->protocol_.feed(buffer[i], read_time);
    }
  }

  const uint32_t now = millis();
  if (uart_idle) {
    this->protocol_.check_timeout(now);
  }

  if (this->wifi_control_mode_ == WIFI_CONTROL_MODE_MCU) {
    this->handle_reset_button_(now);
    this->update_wifi_led_(now);
  }
}

void TuyaWifiMcuComponent::update() { this->report_tuya_dp_states(); }

void TuyaWifiMcuComponent::send_frame_(TuyaCommand command, const uint8_t *payload, uint16_t payload_length) {
  const size_t frame_length =
      encode_tuya_frame(command, payload, payload_length, this->tx_buffer_.data(), this->tx_buffer_.size());
  if (frame_length == 0) {
    ESP_LOGW(TAG, "Could not encode Tuya command 0x%02X", static_cast<uint8_t>(command));
    return;
  }
  this->write_array(this->tx_buffer_.data(), frame_length);
}

void TuyaWifiMcuComponent::on_tuya_frame(const TuyaFrame &frame) {
  ESP_LOGV(TAG, "Received Tuya command 0x%02X with %u payload bytes", static_cast<uint8_t>(frame.command),
           frame.payload_length);

  switch (frame.command) {
    case TuyaCommand::HEARTBEAT: {
      if (frame.payload_length != 0) {
        return;
      }
      const uint8_t heartbeat = this->first_heartbeat_ ? 0x00 : 0x01;
      this->first_heartbeat_ = false;
      this->send_frame_(TuyaCommand::HEARTBEAT, &heartbeat, 1);
      break;
    }

    case TuyaCommand::PRODUCT_QUERY: {
      if (frame.payload_length != 0) {
        return;
      }
      std::array<char, 64> product_info{};
      const int length = snprintf(product_info.data(), product_info.size(), "{\"p\":\"%s\",\"v\":\"%s\",\"m\":0}",
                                  this->product_id_.c_str(), this->mcu_version_.c_str());
      if (length <= 0 || static_cast<size_t>(length) >= product_info.size()) {
        ESP_LOGE(TAG, "Tuya product information is too long");
        return;
      }
      this->send_frame_(TuyaCommand::PRODUCT_QUERY, reinterpret_cast<const uint8_t *>(product_info.data()),
                        static_cast<uint16_t>(length));
      break;
    }

    case TuyaCommand::WORK_MODE_QUERY: {
      if (frame.payload_length != 0) {
        return;
      }
      if (this->wifi_control_mode_ == WIFI_CONTROL_MODE_MODULE) {
        const uint8_t pins[] = {this->module_wifi_led_pin_, this->module_wifi_reset_pin_};
        this->send_frame_(TuyaCommand::WORK_MODE_QUERY, pins, sizeof(pins));
      } else {
        this->send_frame_(TuyaCommand::WORK_MODE_QUERY);
      }
      break;
    }

    case TuyaCommand::WIFI_STATE:
      if (frame.payload_length != 1 || frame.payload[0] > 0x06) {
        return;
      }
      this->wifi_work_state_ = frame.payload[0];
      this->send_frame_(TuyaCommand::WIFI_STATE);
      break;

    case TuyaCommand::WIFI_RESET:
      ESP_LOGD(TAG, "Tuya WiFi reset acknowledged");
      break;

    case TuyaCommand::WIFI_MODE:
      ESP_LOGD(TAG, "Tuya WiFi mode change acknowledged");
      break;

    case TuyaCommand::DP_DOWNLOAD:
      this->process_dp_download_(frame.payload, frame.payload_length);
      break;

    case TuyaCommand::DP_UPLOAD:
      break;

    case TuyaCommand::STATE_QUERY:
      if (frame.payload_length == 0) {
        this->report_tuya_dp_states();
      }
      break;

    default:
      ESP_LOGD(TAG, "Ignoring unsupported Tuya command 0x%02X", static_cast<uint8_t>(frame.command));
      break;
  }
}

bool TuyaWifiMcuComponent::validate_dp_payload_(const uint8_t *payload, uint16_t payload_length) const {
  uint16_t offset = 0;
  while (offset < payload_length) {
    if (payload_length - offset < 4) {
      return false;
    }
    const auto type = static_cast<TuyaDpType>(payload[offset + 1]);
    const uint16_t length = (static_cast<uint16_t>(payload[offset + 2]) << 8) | payload[offset + 3];
    offset += 4;
    if (length > payload_length - offset) {
      return false;
    }

    switch (type) {
      case TuyaDpType::RAW:
      case TuyaDpType::STRING:
        break;
      case TuyaDpType::BOOLEAN:
        if (length != 1 || payload[offset] > 1) {
          return false;
        }
        break;
      case TuyaDpType::VALUE:
        if (length != 4) {
          return false;
        }
        break;
      case TuyaDpType::ENUM:
        if (length != 1) {
          return false;
        }
        break;
      case TuyaDpType::BITMAP:
        if (length != 1 && length != 2 && length != 4) {
          return false;
        }
        break;
      default:
        return false;
    }
    offset += length;
  }
  return offset == payload_length && payload_length != 0;
}

void TuyaWifiMcuComponent::process_dp_download_(const uint8_t *payload, uint16_t payload_length) {
  if (!this->validate_dp_payload_(payload, payload_length)) {
    ESP_LOGW(TAG, "Ignoring malformed Tuya DP download");
    return;
  }

  uint16_t offset = 0;
  while (offset < payload_length) {
    const uint8_t dp_id = payload[offset];
    const auto type = static_cast<TuyaDpType>(payload[offset + 1]);
    const uint16_t length = (static_cast<uint16_t>(payload[offset + 2]) << 8) | payload[offset + 3];
    const uint8_t *value = payload + offset + 4;
    offset += 4 + length;

    const auto result = dispatch_tuya_dp(this->entities_, dp_id, type, value, length);
    if (result.id_matches == 0) {
      ESP_LOGW(TAG, "Ignoring unknown Tuya DP %u", dp_id);
    } else if (result.type_matches == 0) {
      ESP_LOGW(TAG, "Ignoring Tuya DP %u with unexpected type %u", dp_id, static_cast<uint8_t>(type));
    } else if (result.accepted == 0) {
      ESP_LOGW(TAG, "Tuya DP %u was rejected by all matching entities", dp_id);
    }
  }
}

void TuyaWifiMcuComponent::report_bool_dp(uint8_t dp_id, bool value) {
  const uint8_t payload[] = {dp_id, static_cast<uint8_t>(TuyaDpType::BOOLEAN), 0x00, 0x01,
                             static_cast<uint8_t>(value)};
  this->send_frame_(TuyaCommand::DP_UPLOAD, payload, sizeof(payload));
}

void TuyaWifiMcuComponent::report_value_dp(uint8_t dp_id, uint32_t value) {
  uint8_t payload[] = {dp_id, static_cast<uint8_t>(TuyaDpType::VALUE), 0x00, 0x04, 0x00, 0x00, 0x00, 0x00};
  encode_tuya_value(value, payload + 4);
  this->send_frame_(TuyaCommand::DP_UPLOAD, payload, sizeof(payload));
}

void TuyaWifiMcuComponent::report_tuya_dp_states() {
  for (auto *entity : this->entities_) {
    if (entity != nullptr) {
      entity->report_tuya_dp_state();
    }
  }
}

void TuyaWifiMcuComponent::reset_tuya_wifi() { this->send_frame_(TuyaCommand::WIFI_RESET); }

void TuyaWifiMcuComponent::handle_reset_button_(uint32_t now) {
  if (this->wifi_reset_pin_ == nullptr) {
    return;
  }

  const bool pressed = this->wifi_reset_pin_->digital_read();
  if (pressed != this->reset_raw_pressed_) {
    this->reset_raw_pressed_ = pressed;
    this->reset_transition_time_ = now;
  }

  if (pressed != this->reset_stable_pressed_ && now - this->reset_transition_time_ >= RESET_DEBOUNCE_MS) {
    this->reset_stable_pressed_ = pressed;
    if (pressed && !this->reset_press_handled_) {
      ESP_LOGD(TAG, "Starting Tuya smart config");
      this->send_frame_(TuyaCommand::WIFI_MODE, &SMART_CONFIG, 1);
      this->reset_press_handled_ = true;
    } else if (!pressed) {
      this->reset_press_handled_ = false;
    }
  }
}

void TuyaWifiMcuComponent::update_wifi_led_(uint32_t now) {
  if (this->wifi_led_pin_ == nullptr) {
    return;
  }

  if (this->wifi_work_state_ == WIFI_CONN_CLOUD) {
    if (!this->wifi_led_state_) {
      this->wifi_led_state_ = true;
      this->wifi_led_pin_->digital_write(true);
    }
    return;
  }

  if (this->wifi_work_state_ == WIFI_LOW_POWER || this->wifi_work_state_ == 0xFF) {
    if (this->wifi_led_state_) {
      this->wifi_led_state_ = false;
      this->wifi_led_pin_->digital_write(false);
    }
    return;
  }

  if (now - this->last_wifi_led_state_change_time_ >= WIFI_LED_BLINK_INTERVAL_MS) {
    this->last_wifi_led_state_change_time_ = now;
    this->wifi_led_state_ = !this->wifi_led_state_;
    this->wifi_led_pin_->digital_write(this->wifi_led_state_);
  }
}

float TuyaWifiMcuComponent::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

}  // namespace tuya_wifi_mcu
}  // namespace esphome

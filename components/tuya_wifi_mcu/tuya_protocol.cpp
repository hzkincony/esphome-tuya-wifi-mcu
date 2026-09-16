#include "tuya_protocol.h"

namespace esphome {
namespace tuya_wifi_mcu {

void TuyaProtocolParser::reset() {
  this->state_ = State::WAIT_HEADER_55;
  this->payload_length_ = 0;
  this->payload_position_ = 0;
  this->skip_remaining_ = 0;
  this->checksum_ = 0;
  this->frame_active_ = false;
}

void TuyaProtocolParser::start_frame_(uint32_t now) {
  this->state_ = State::WAIT_HEADER_AA;
  this->payload_length_ = 0;
  this->payload_position_ = 0;
  this->checksum_ = 0x55;
  this->last_byte_time_ = now;
  this->frame_active_ = true;
}

void TuyaProtocolParser::reset_with_byte_(uint8_t byte, uint32_t now) {
  this->reset();
  if (byte == 0x55) {
    this->start_frame_(now);
  }
}

void TuyaProtocolParser::feed(uint8_t byte, uint32_t now) {
  this->last_byte_time_ = now;

  switch (this->state_) {
    case State::WAIT_HEADER_55:
      if (byte == 0x55) {
        this->start_frame_(now);
      }
      return;

    case State::WAIT_HEADER_AA:
      if (byte == 0xAA) {
        this->checksum_ += byte;
        this->state_ = State::READ_VERSION;
      } else if (byte == 0x55) {
        this->start_frame_(now);
      } else {
        this->reset();
      }
      return;

    case State::READ_VERSION:
      if (byte != 0x00) {
        this->version_errors_++;
        this->reset_with_byte_(byte, now);
        return;
      }
      this->version_ = byte;
      this->checksum_ += byte;
      this->state_ = State::READ_COMMAND;
      return;

    case State::READ_COMMAND:
      this->command_ = byte;
      this->checksum_ += byte;
      this->state_ = State::READ_LENGTH_HIGH;
      return;

    case State::READ_LENGTH_HIGH:
      this->payload_length_ = static_cast<uint16_t>(byte) << 8;
      this->checksum_ += byte;
      this->state_ = State::READ_LENGTH_LOW;
      return;

    case State::READ_LENGTH_LOW:
      this->payload_length_ |= byte;
      this->checksum_ += byte;
      if (this->payload_length_ > MAX_PAYLOAD_SIZE) {
        this->oversize_errors_++;
        this->skip_remaining_ = static_cast<uint32_t>(this->payload_length_) + 1;
        this->state_ = State::SKIP_OVERSIZE;
        return;
      }
      this->payload_position_ = 0;
      this->state_ = this->payload_length_ == 0 ? State::READ_CHECKSUM : State::READ_PAYLOAD;
      return;

    case State::READ_PAYLOAD:
      this->payload_[this->payload_position_++] = byte;
      this->checksum_ += byte;
      if (this->payload_position_ == this->payload_length_) {
        this->state_ = State::READ_CHECKSUM;
      }
      return;

    case State::READ_CHECKSUM:
      if (byte == this->checksum_) {
        if (this->listener_ != nullptr) {
          const TuyaFrame frame{this->version_, static_cast<TuyaCommand>(this->command_), this->payload_.data(),
                                this->payload_length_};
          this->listener_->on_tuya_frame(frame);
        }
        this->reset();
      } else {
        this->checksum_errors_++;
        this->reset_with_byte_(byte, now);
      }
      return;

    case State::SKIP_OVERSIZE:
      if (--this->skip_remaining_ == 0) {
        this->reset();
      }
      return;
  }
}

void TuyaProtocolParser::check_timeout(uint32_t now) {
  if (this->frame_active_ && now - this->last_byte_time_ > INTER_BYTE_TIMEOUT_MS) {
    this->timeout_errors_++;
    this->reset();
  }
}

size_t encode_tuya_frame(TuyaCommand command, const uint8_t *payload, uint16_t payload_length, uint8_t *output,
                         size_t output_capacity) {
  if (payload_length > TuyaProtocolParser::MAX_PAYLOAD_SIZE || (payload_length != 0 && payload == nullptr) ||
      output == nullptr) {
    return 0;
  }

  const size_t frame_length = static_cast<size_t>(payload_length) + 7;
  if (output_capacity < frame_length) {
    return 0;
  }

  output[0] = 0x55;
  output[1] = 0xAA;
  output[2] = 0x03;
  output[3] = static_cast<uint8_t>(command);
  output[4] = static_cast<uint8_t>(payload_length >> 8);
  output[5] = static_cast<uint8_t>(payload_length & 0xFF);

  uint8_t checksum = 0;
  for (size_t i = 0; i < 6; i++) {
    checksum += output[i];
  }
  for (uint16_t i = 0; i < payload_length; i++) {
    output[6 + i] = payload[i];
    checksum += payload[i];
  }
  output[6 + payload_length] = checksum;
  return frame_length;
}

uint32_t decode_tuya_value(const uint8_t *data) {
  return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

void encode_tuya_value(uint32_t value, uint8_t *data) {
  data[0] = static_cast<uint8_t>(value >> 24);
  data[1] = static_cast<uint8_t>(value >> 16);
  data[2] = static_cast<uint8_t>(value >> 8);
  data[3] = static_cast<uint8_t>(value);
}

}  // namespace tuya_wifi_mcu
}  // namespace esphome

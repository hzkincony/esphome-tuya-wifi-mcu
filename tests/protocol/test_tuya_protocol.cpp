#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../../components/tuya_wifi_mcu/tuya_protocol.h"

using esphome::tuya_wifi_mcu::TuyaCommand;
using esphome::tuya_wifi_mcu::TuyaFrame;
using esphome::tuya_wifi_mcu::TuyaProtocolListener;
using esphome::tuya_wifi_mcu::TuyaProtocolParser;
using esphome::tuya_wifi_mcu::decode_tuya_value;
using esphome::tuya_wifi_mcu::encode_tuya_frame;
using esphome::tuya_wifi_mcu::encode_tuya_value;

class Listener : public TuyaProtocolListener {
 public:
  void on_tuya_frame(const TuyaFrame &frame) override {
    this->commands.push_back(frame.command);
    this->payloads.emplace_back(frame.payload, frame.payload + frame.payload_length);
  }

  std::vector<TuyaCommand> commands;
  std::vector<std::vector<uint8_t>> payloads;
};

static void feed(TuyaProtocolParser &parser, const std::vector<uint8_t> &data, uint32_t start = 0) {
  uint32_t now = start;
  for (uint8_t byte : data) {
    parser.feed(byte, now++);
  }
}

static void test_encoder() {
  uint8_t output[300];
  const uint8_t first_heartbeat[] = {0x00};
  size_t length = encode_tuya_frame(TuyaCommand::HEARTBEAT, first_heartbeat, 1, output, sizeof(output));
  const uint8_t expected_heartbeat[] = {0x55, 0xAA, 0x03, 0x00, 0x00, 0x01, 0x00, 0x03};
  assert(length == sizeof(expected_heartbeat));
  assert(std::memcmp(output, expected_heartbeat, length) == 0);

  const uint8_t bool_dp[] = {0x01, 0x01, 0x00, 0x01, 0x01};
  length = encode_tuya_frame(TuyaCommand::DP_UPLOAD, bool_dp, sizeof(bool_dp), output, sizeof(output));
  const uint8_t expected_bool[] = {0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x01, 0x01, 0x00, 0x01, 0x01, 0x12};
  assert(length == sizeof(expected_bool));
  assert(std::memcmp(output, expected_bool, length) == 0);

  uint8_t value[4];
  encode_tuya_value(100, value);
  const uint8_t expected_value[] = {0x00, 0x00, 0x00, 0x64};
  assert(std::memcmp(value, expected_value, sizeof(value)) == 0);
  assert(decode_tuya_value(value) == 100);

  assert(encode_tuya_frame(TuyaCommand::HEARTBEAT, nullptr, 1, output, sizeof(output)) == 0);
  assert(encode_tuya_frame(TuyaCommand::HEARTBEAT, nullptr, 0, output, 6) == 0);
}

static void test_parser() {
  Listener listener;
  TuyaProtocolParser parser(&listener);

  const std::vector<uint8_t> heartbeat = {0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0xFF};
  feed(parser, heartbeat);
  assert(listener.commands.size() == 1);
  assert(listener.commands[0] == TuyaCommand::HEARTBEAT);
  assert(listener.payloads[0].empty());

  const std::vector<uint8_t> bool_dp = {0x55, 0xAA, 0x00, 0x06, 0x00, 0x05, 0x01, 0x01, 0x00, 0x01, 0x01, 0x0E};
  feed(parser, {0x12, 0x34, 0x55});
  feed(parser, std::vector<uint8_t>(bool_dp.begin() + 1, bool_dp.end()), 10);
  assert(listener.commands.size() == 2);
  assert(listener.commands[1] == TuyaCommand::DP_DOWNLOAD);
  assert(listener.payloads[1] == std::vector<uint8_t>({0x01, 0x01, 0x00, 0x01, 0x01}));

  std::vector<uint8_t> combined = heartbeat;
  combined.insert(combined.end(), heartbeat.begin(), heartbeat.end());
  feed(parser, combined, 30);
  assert(listener.commands.size() == 4);

  auto bad_checksum = heartbeat;
  bad_checksum.back() = 0x00;
  feed(parser, bad_checksum, 50);
  assert(listener.commands.size() == 4);
  assert(parser.get_checksum_errors() == 1);

  feed(parser, {0x55, 0xAA, 0x01}, 70);
  assert(parser.get_version_errors() == 1);

  feed(parser, {0x55, 0xAA, 0x00, 0x06, 0x01, 0x01}, 80);
  assert(parser.get_oversize_errors() == 1);

  feed(parser, {0x55, 0xAA, 0x00}, 90);
  parser.check_timeout(200);
  assert(parser.get_timeout_errors() == 1);
  feed(parser, heartbeat, 210);
  assert(listener.commands.size() == 5);
}

int main() {
  test_encoder();
  test_parser();
  return 0;
}

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

static std::vector<uint8_t> make_rx_frame(TuyaCommand command, const std::vector<uint8_t> &payload) {
  const uint16_t length = payload.size();
  std::vector<uint8_t> frame = {0x55, 0xAA, 0x00, static_cast<uint8_t>(command),
                                static_cast<uint8_t>(length >> 8), static_cast<uint8_t>(length)};
  frame.insert(frame.end(), payload.begin(), payload.end());
  uint8_t checksum = 0;
  for (uint8_t byte : frame) {
    checksum += byte;
  }
  frame.push_back(checksum);
  return frame;
}

static void feed(TuyaProtocolParser &parser, const std::vector<uint8_t> &data, uint32_t now = 0) {
  for (uint8_t byte : data) {
    parser.feed(byte, now);
  }
}

static void test_encoder() {
  std::vector<uint8_t> output(TuyaProtocolParser::MAX_PAYLOAD_SIZE + 7);
  const uint8_t first_heartbeat[] = {0x00};
  size_t length =
      encode_tuya_frame(TuyaCommand::HEARTBEAT, first_heartbeat, 1, output.data(), output.size());
  const uint8_t expected_heartbeat[] = {0x55, 0xAA, 0x03, 0x00, 0x00, 0x01, 0x00, 0x03};
  assert(length == sizeof(expected_heartbeat));
  assert(std::memcmp(output.data(), expected_heartbeat, length) == 0);

  const uint8_t bool_dp[] = {0x01, 0x01, 0x00, 0x01, 0x01};
  length = encode_tuya_frame(TuyaCommand::DP_UPLOAD, bool_dp, sizeof(bool_dp), output.data(), output.size());
  const uint8_t expected_bool[] = {0x55, 0xAA, 0x03, 0x07, 0x00, 0x05, 0x01, 0x01, 0x00, 0x01, 0x01, 0x12};
  assert(length == sizeof(expected_bool));
  assert(std::memcmp(output.data(), expected_bool, length) == 0);

  uint8_t value[4];
  encode_tuya_value(100, value);
  const uint8_t expected_value[] = {0x00, 0x00, 0x00, 0x64};
  assert(std::memcmp(value, expected_value, sizeof(value)) == 0);
  assert(decode_tuya_value(value) == 100);

  std::vector<uint8_t> max_payload(TuyaProtocolParser::MAX_PAYLOAD_SIZE, 0x5A);
  assert(encode_tuya_frame(TuyaCommand::DP_UPLOAD, max_payload.data(), max_payload.size(), output.data(),
                           output.size()) == output.size());
  assert(encode_tuya_frame(TuyaCommand::DP_UPLOAD, max_payload.data(), max_payload.size() + 1, output.data(),
                           output.size()) == 0);
  assert(encode_tuya_frame(TuyaCommand::HEARTBEAT, nullptr, 1, output.data(), output.size()) == 0);
  assert(encode_tuya_frame(TuyaCommand::HEARTBEAT, nullptr, 0, output.data(), 6) == 0);
}

static void test_parser_basics() {
  Listener listener;
  TuyaProtocolParser parser(&listener);
  const auto heartbeat = make_rx_frame(TuyaCommand::HEARTBEAT, {});

  feed(parser, heartbeat);
  assert(listener.commands.size() == 1);
  assert(listener.commands[0] == TuyaCommand::HEARTBEAT);
  assert(listener.payloads[0].empty());

  const std::vector<uint8_t> bool_payload = {0x01, 0x01, 0x00, 0x01, 0x01};
  const auto bool_dp = make_rx_frame(TuyaCommand::DP_DOWNLOAD, bool_payload);
  feed(parser, {0x12, 0x34, 0x55});
  feed(parser, std::vector<uint8_t>(bool_dp.begin() + 1, bool_dp.end()), 10);
  assert(listener.commands.size() == 2);
  assert(listener.payloads[1] == bool_payload);

  auto bad_checksum = heartbeat;
  bad_checksum.back() = 0x00;
  feed(parser, bad_checksum, 50);
  assert(listener.commands.size() == 2);
  assert(parser.get_checksum_errors() == 1);

  feed(parser, {0x55, 0xAA, 0x01}, 70);
  assert(parser.get_version_errors() == 1);
}

static void test_timeout_is_explicit() {
  Listener listener;
  TuyaProtocolParser parser(&listener);
  std::vector<uint8_t> payload(80, 0x42);
  const auto frame = make_rx_frame(TuyaCommand::DP_DOWNLOAD, payload);

  feed(parser, std::vector<uint8_t>(frame.begin(), frame.begin() + 64), 0);
  feed(parser, std::vector<uint8_t>(frame.begin() + 64, frame.end()), 150);
  assert(listener.payloads.size() == 1);
  assert(listener.payloads[0] == payload);
  assert(parser.get_timeout_errors() == 0);

  feed(parser, std::vector<uint8_t>(frame.begin(), frame.begin() + 64), 200);
  parser.check_timeout(301);
  assert(parser.get_timeout_errors() == 1);
  feed(parser, make_rx_frame(TuyaCommand::HEARTBEAT, {}), 302);
  assert(listener.commands.size() == 2);
  assert(listener.commands.back() == TuyaCommand::HEARTBEAT);
}

static void test_payload_limits_and_recovery() {
  Listener listener;
  TuyaProtocolParser parser(&listener);

  std::vector<uint8_t> max_payload(TuyaProtocolParser::MAX_PAYLOAD_SIZE, 0x5A);
  feed(parser, make_rx_frame(TuyaCommand::DP_DOWNLOAD, max_payload));
  assert(listener.payloads.size() == 1);
  assert(listener.payloads[0] == max_payload);

  std::vector<uint8_t> aggregated;
  for (uint8_t dp_id = 1; dp_id <= 33; dp_id++) {
    aggregated.insert(aggregated.end(), {dp_id, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, dp_id});
  }
  assert(aggregated.size() == 264);
  feed(parser, make_rx_frame(TuyaCommand::DP_DOWNLOAD, aggregated));
  assert(listener.payloads.size() == 2);
  assert(listener.payloads[1] == aggregated);

  const uint16_t oversize_length = TuyaProtocolParser::MAX_PAYLOAD_SIZE + 1;
  std::vector<uint8_t> oversize = {0x55, 0xAA, 0x00, 0x06, static_cast<uint8_t>(oversize_length >> 8),
                                   static_cast<uint8_t>(oversize_length)};
  for (uint16_t i = 0; i < oversize_length; i++) {
    oversize.push_back(i % 17 == 0 ? 0x55 : (i % 17 == 1 ? 0xAA : 0x33));
  }
  oversize.push_back(0x00);
  const auto heartbeat = make_rx_frame(TuyaCommand::HEARTBEAT, {});
  oversize.insert(oversize.end(), heartbeat.begin(), heartbeat.end());
  feed(parser, oversize);
  assert(parser.get_oversize_errors() == 1);
  assert(listener.commands.size() == 3);
  assert(listener.commands.back() == TuyaCommand::HEARTBEAT);

  std::vector<uint8_t> maximum_declared = {0x55, 0xAA, 0x00, 0x06, 0xFF, 0xFF};
  maximum_declared.resize(maximum_declared.size() + 65535, 0x55);
  maximum_declared.push_back(0x00);
  maximum_declared.insert(maximum_declared.end(), heartbeat.begin(), heartbeat.end());
  feed(parser, maximum_declared);
  assert(parser.get_oversize_errors() == 2);
  assert(listener.commands.size() == 4);
  assert(listener.commands.back() == TuyaCommand::HEARTBEAT);
}

int main() {
  test_encoder();
  test_parser_basics();
  test_timeout_is_explicit();
  test_payload_limits_and_recovery();
  return 0;
}

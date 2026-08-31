#include <cassert>
#include <cstdint>
#include <vector>

#include "../../components/tuya_wifi_mcu/tuya_brightness.h"
#include "../../components/tuya_wifi_mcu/tuya_dp_dispatch.h"

using esphome::tuya_wifi_mcu::TuyaDpType;
using esphome::tuya_wifi_mcu::TuyaWifiMcuEntity;
using esphome::tuya_wifi_mcu::dispatch_tuya_dp;
using esphome::tuya_wifi_mcu::tuya_brightness_from_linear;

class FakeEntity : public TuyaWifiMcuEntity {
 public:
  explicit FakeEntity(TuyaDpType type, bool accept = true) : type_(type), accept_(accept) {}

  TuyaDpType get_dp_type() const override { return this->type_; }
  void report_tuya_dp_state() override { this->reports_++; }
  bool process_dp_data(const uint8_t *, uint16_t) override {
    this->processes_++;
    for (auto *peer : this->peers_) {
      assert(peer->is_processing_remote());
    }
    return this->accept_;
  }

  void set_peers(const std::vector<FakeEntity *> &peers) { this->peers_ = peers; }
  size_t processes() const { return this->processes_; }
  size_t reports() const { return this->reports_; }

 protected:
  TuyaDpType type_;
  bool accept_;
  std::vector<FakeEntity *> peers_;
  size_t processes_{0};
  size_t reports_{0};
};

static void test_brightness() {
  assert(tuya_brightness_from_linear(-1.0f) == 0);
  assert(tuya_brightness_from_linear(0.0f) == 0);
  assert(tuya_brightness_from_linear(0.004f) == 0);
  assert(tuya_brightness_from_linear(0.005f) == 1);
  assert(tuya_brightness_from_linear(0.5f) == 50);
  assert(tuya_brightness_from_linear(1.0f) == 100);
  assert(tuya_brightness_from_linear(2.0f) == 100);
}

static void test_duplicate_dp_dispatch() {
  FakeEntity first(TuyaDpType::BOOLEAN);
  FakeEntity second(TuyaDpType::BOOLEAN);
  FakeEntity different_type(TuyaDpType::VALUE);
  first.set_dp_id(1);
  second.set_dp_id(1);
  different_type.set_dp_id(1);
  const std::vector<FakeEntity *> peers = {&first, &second, &different_type};
  first.set_peers(peers);
  second.set_peers(peers);
  different_type.set_peers(peers);

  std::vector<TuyaWifiMcuEntity *> entities(peers.begin(), peers.end());
  const uint8_t value = 1;
  const auto result = dispatch_tuya_dp(entities, 1, TuyaDpType::BOOLEAN, &value, 1);
  assert(result.id_matches == 3);
  assert(result.type_matches == 2);
  assert(result.accepted == 2);
  assert(first.processes() == 1);
  assert(second.processes() == 1);
  assert(different_type.processes() == 0);
  assert(first.reports() + second.reports() == 1);
  assert(!first.is_processing_remote());
  assert(!second.is_processing_remote());
  assert(!different_type.is_processing_remote());
}

static void test_partial_and_rejected_dispatch() {
  FakeEntity rejected(TuyaDpType::BOOLEAN, false);
  FakeEntity accepted(TuyaDpType::BOOLEAN, true);
  rejected.set_dp_id(2);
  accepted.set_dp_id(2);
  const std::vector<FakeEntity *> peers = {&rejected, &accepted};
  rejected.set_peers(peers);
  accepted.set_peers(peers);
  std::vector<TuyaWifiMcuEntity *> entities(peers.begin(), peers.end());
  const uint8_t value = 0;

  auto result = dispatch_tuya_dp(entities, 2, TuyaDpType::BOOLEAN, &value, 1);
  assert(result.accepted == 1);
  assert(rejected.reports() == 0);
  assert(accepted.reports() == 1);

  result = dispatch_tuya_dp(entities, 2, TuyaDpType::VALUE, &value, 1);
  assert(result.id_matches == 2);
  assert(result.type_matches == 0);
  assert(result.accepted == 0);

  result = dispatch_tuya_dp(entities, 3, TuyaDpType::BOOLEAN, &value, 1);
  assert(result.id_matches == 0);
  assert(result.type_matches == 0);
  assert(result.accepted == 0);
}

int main() {
  test_brightness();
  test_duplicate_dp_dispatch();
  test_partial_and_rejected_dispatch();
  return 0;
}

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

#include "command.hpp"
#include "snapshot.hpp"

namespace {

enum class Mix { Balanced, BuyHeavy, SellHeavy };

int BuyPercent(Mix mix) {
  switch (mix) {
    case Mix::BuyHeavy:
      return 80;
    case Mix::SellHeavy:
      return 20;
    case Mix::Balanced:
      return 50;
  }
  return 50;
}

lob::test::Commands Generate(std::uint32_t seed, std::size_t length, Mix mix) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> percent(0, 99);
  std::uniform_int_distribution<lob::PriceTicks> price(95, 105);
  std::uniform_int_distribution<lob::Quantity> quantity(1, 8);
  const int buy_percent = BuyPercent(mix);

  lob::test::Commands commands;
  commands.reserve(length);
  std::vector<lob::OrderId> submitted;
  lob::OrderId next_id = 1;

  for (std::size_t i = 0; i < length; ++i) {
    const bool can_cancel = !submitted.empty();
    const int roll = percent(rng);
    if (!can_cancel || roll < 70) {
      const int kind = percent(rng);
      if (kind < 3) {
        commands.push_back(lob::test::SubmitGtc(next_id, lob::Side::Buy, price(rng), 0));
        ++next_id;
      } else if (kind < 8 && !submitted.empty()) {
        const lob::OrderId dup =
            submitted[static_cast<std::size_t>(percent(rng)) % submitted.size()];
        const lob::Side side =
            percent(rng) < buy_percent ? lob::Side::Buy : lob::Side::Sell;
        commands.push_back(lob::test::SubmitGtc(dup, side, price(rng), quantity(rng)));
      } else {
        const lob::Side side =
            percent(rng) < buy_percent ? lob::Side::Buy : lob::Side::Sell;
        commands.push_back(
            lob::test::SubmitGtc(next_id, side, price(rng), quantity(rng)));
        submitted.push_back(next_id);
        ++next_id;
      }
    } else if (roll < 75) {
      commands.push_back(lob::test::CancelOrder(next_id + 1000));
    } else {
      const lob::OrderId id =
          submitted[static_cast<std::size_t>(percent(rng)) % submitted.size()];
      commands.push_back(lob::test::CancelOrder(id));
    }
  }
  return commands;
}

void RunSeed(std::uint32_t seed, std::size_t length, Mix mix) {
  const lob::test::Commands commands = Generate(seed, length, mix);
  const StreamDiff diff = FirstDiff(commands);
  if (diff.kind == DiffKind::None) {
    return;
  }
  std::cerr << "differential mismatch seed=" << seed << "\n";
  PersistFailure(seed, commands, diff);
  std::abort();
}

}  // namespace

void TestBuyHeavy() {
  RunSeed(1, 200, Mix::BuyHeavy);
}

void TestSellHeavy() {
  RunSeed(2, 200, Mix::SellHeavy);
}

void TestBalanced() {
  RunSeed(3, 200, Mix::Balanced);
}

void TestFixedCampaign() {
  const std::uint32_t seeds[] = {4, 5, 6, 7, 8, 11, 17, 29, 42, 100};
  for (const std::uint32_t seed : seeds) {
    RunSeed(seed, 150, Mix::Balanced);
    RunSeed(seed + 1000, 150, Mix::BuyHeavy);
    RunSeed(seed + 2000, 150, Mix::SellHeavy);
  }
}

int main() {
  TestBuyHeavy();
  TestSellHeavy();
  TestBalanced();
  TestFixedCampaign();
  return 0;
}

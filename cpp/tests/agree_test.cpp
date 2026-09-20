#include <limits>
#include <type_traits>
#include <variant>
#include <vector>

#include "lob/order_book.hpp"
#include "reference_book.hpp"
#include "scenarios/worked_examples.hpp"
#include "snapshot.hpp"

namespace {

constexpr std::size_t kFullDepth = 32;

void RunNonCrossing(const std::vector<lob::NewOrder>& orders) {
  lob::test::Commands commands;
  commands.reserve(orders.size());
  for (const lob::NewOrder& order : orders) {
    commands.push_back(lob::test::SubmitCommand{order});
  }
  RequireStream(commands);
}

void ExpectResting(const lob::test::Commands& commands, lob::OrderId id,
                   lob::Quantity quantity, const char* label) {
  lob::OrderBook production;
  lob::ref::ReferenceBook reference;
  std::vector<lob::OrderId> ids;
  for (const lob::test::Command& command : commands) {
    std::visit(
        [&](const auto& payload) {
          using T = std::decay_t<decltype(payload)>;
          if constexpr (std::is_same_v<T, lob::test::SubmitCommand>) {
            production.Submit(payload.order);
            reference.Submit(payload.order);
            ids.push_back(payload.order.id);
          } else {
            production.Cancel(payload.id);
            reference.Cancel(payload.id);
            ids.push_back(payload.id);
          }
        },
        command);
  }
  if (production.GetRestingQuantity(id) != quantity ||
      reference.GetRestingQuantity(id) != quantity) {
    FailWithHistory(label, commands);
  }
  RequireAgree(label, {}, Capture(production, ids, kFullDepth),
               Capture(reference, ids, kFullDepth));
}

}  // namespace

void TestGoldenNonCrossingRest() {
  RunNonCrossing({
      {1, lob::Side::Buy, 100, 10, lob::TimeInForce::GTC},
  });
}

void TestBidAndAskPriceOrder() {
  RunNonCrossing({
      {1, lob::Side::Buy, 100, 10, lob::TimeInForce::GTC},
      {2, lob::Side::Buy, 101, 5, lob::TimeInForce::GTC},
      {3, lob::Side::Sell, 110, 4, lob::TimeInForce::GTC},
      {4, lob::Side::Sell, 105, 7, lob::TimeInForce::GTC},
  });
}

void TestSamePriceSumsLevel() {
  RunNonCrossing({
      {1, lob::Side::Buy, 100, 10, lob::TimeInForce::GTC},
      {2, lob::Side::Buy, 100, 5, lob::TimeInForce::GTC},
  });
}

void TestSellOnlyBook() {
  RunNonCrossing({
      {1, lob::Side::Sell, 120, 3, lob::TimeInForce::GTC},
      {2, lob::Side::Sell, 115, 8, lob::TimeInForce::GTC},
  });
}

void TestGoldenCrossAtMakerPrice() {
  RequireStream(lob::test::GoldenCrossAtMakerPrice());
}

void TestGoldenFifoAtOnePrice() {
  RequireStream(lob::test::GoldenFifoAtOnePrice());
  ExpectResting(lob::test::GoldenFifoAtOnePrice(), 2, 4,
                "fifo leftover should rest on order 2");
}

void TestTakerRemainderRests() {
  RequireStream({
      lob::test::SubmitGtc(1, lob::Side::Sell, 100, 3),
      lob::test::SubmitGtc(2, lob::Side::Buy, 101, 10),
  });
}

void TestSweepStopsAtUnacceptablePrice() {
  RequireStream({
      lob::test::SubmitGtc(1, lob::Side::Sell, 100, 3),
      lob::test::SubmitGtc(2, lob::Side::Sell, 110, 5),
      lob::test::SubmitGtc(3, lob::Side::Buy, 101, 10),
  });
}

void TestIncomingSellMatchesHighestBid() {
  RequireStream({
      lob::test::SubmitGtc(1, lob::Side::Buy, 100, 4),
      lob::test::SubmitGtc(2, lob::Side::Buy, 101, 6),
      lob::test::SubmitGtc(3, lob::Side::Sell, 100, 8),
  });
}

void TestPlainEnglishWorkedExample() {
  const lob::test::Commands commands = lob::test::PlainEnglishWorkedExample();
  RequireStream(commands);
  ExpectResting(commands, 11, 2, "plain-english leftover on order 11");
  ExpectResting(commands, 12, 10, "plain-english order 12 untouched");
  ExpectResting(commands, 10, 0, "plain-english order 10 filled");
  ExpectResting(commands, 20, 0, "plain-english taker filled");
}

void TestTechnicalNumericExample() {
  const lob::test::Commands commands = lob::test::TechnicalNumericExample();
  RequireStream(commands);
  ExpectResting(commands, 7, 90, "numeric example remainder rests at 10150");
  ExpectResting(commands, 1, 0, "numeric example cancelled the 10050 bid");
  ExpectResting(commands, 2, 80, "numeric example 10000 bid untouched");
}

void TestCancelPreservesSurvivorFifo() {
  RequireStream(lob::test::CancelHeadMiddleTail());
}

void TestUnknownAndDuplicateRejects() {
  RequireStream({
      lob::test::CancelOrder(9),
      lob::test::SubmitGtc(1, lob::Side::Buy, 100, 10),
      lob::test::SubmitGtc(1, lob::Side::Sell, 90, 4),
      lob::test::SubmitGtc(2, lob::Side::Buy, 100, 0),
      lob::test::CancelOrder(1),
      lob::test::CancelOrder(1),
  });
}

void TestGoldenIocCancelsRemainder() {
  RequireStream(lob::test::GoldenIocCancelsRemainder());
  ExpectResting(lob::test::GoldenIocCancelsRemainder(), 2, 0,
                "ioc remainder must not rest");
}

void TestGoldenMarketIgnoresPrice() {
  RequireStream(lob::test::GoldenMarketIgnoresPrice());
  ExpectResting(lob::test::GoldenMarketIgnoresPrice(), 3, 0,
                "market remainder must not rest");
}

void TestGoldenFokRejectsWithoutMutation() {
  RequireStream(lob::test::GoldenFokRejectsWithoutMutation());
  ExpectResting(lob::test::GoldenFokRejectsWithoutMutation(), 1, 3,
                "insufficient fok must leave maker 1");
  ExpectResting(lob::test::GoldenFokRejectsWithoutMutation(), 2, 5,
                "insufficient fok must leave maker 2");
}

void TestEmptyAndOneSidedBooks() {
  RequireStream(lob::test::EmptyBookIoc());
  RequireStream(lob::test::EmptyBookMarket());
  RequireStream(lob::test::EmptyBookFok());
  RequireStream(lob::test::OneSidedIocLeavesOpposite());
  ExpectResting(lob::test::OneSidedIocLeavesOpposite(), 1, 5,
                "one-sided ioc must leave the resting bid");
}

void TestFokOverflowDoesNotWrapAvailability() {
  const lob::Quantity huge = std::numeric_limits<lob::Quantity>::max() / 2 + 10;
  RequireStream({
      lob::test::SubmitGtc(1, lob::Side::Sell, 100, huge),
      lob::test::SubmitGtc(2, lob::Side::Sell, 101, huge),
      lob::test::SubmitOrder(3, lob::Side::Buy, 101, 100, lob::TimeInForce::FOK),
  });
}

int main() {
  TestGoldenNonCrossingRest();
  TestBidAndAskPriceOrder();
  TestSamePriceSumsLevel();
  TestSellOnlyBook();
  TestGoldenCrossAtMakerPrice();
  TestGoldenFifoAtOnePrice();
  TestTakerRemainderRests();
  TestSweepStopsAtUnacceptablePrice();
  TestIncomingSellMatchesHighestBid();
  TestPlainEnglishWorkedExample();
  TestTechnicalNumericExample();
  TestCancelPreservesSurvivorFifo();
  TestUnknownAndDuplicateRejects();
  TestGoldenIocCancelsRemainder();
  TestGoldenMarketIgnoresPrice();
  TestGoldenFokRejectsWithoutMutation();
  TestEmptyAndOneSidedBooks();
  TestFokOverflowDoesNotWrapAvailability();
  return 0;
}

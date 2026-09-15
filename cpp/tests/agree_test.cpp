#include <vector>

#include "lob/order_book.hpp"
#include "reference_book.hpp"
#include "snapshot.hpp"

namespace {

constexpr std::size_t kFullDepth = 32;

void RestProduction(lob::OrderBook& book, const lob::NewOrder& order) {
  book.AddOrder(lob::Order{order.id, order.side, order.price, order.quantity});
}

void RunNonCrossing(const std::vector<lob::NewOrder>& commands) {
  lob::OrderBook production;
  lob::ref::ReferenceBook reference;
  std::vector<lob::NewOrder> history;
  std::vector<lob::OrderId> live;

  RequireAgree("empty book", history, Capture(production, live, kFullDepth),
               Capture(reference, live, kFullDepth));

  for (const lob::NewOrder& order : commands) {
    history.push_back(order);
    RestProduction(production, order);
    const std::vector<lob::Event> events = reference.Submit(order);
    if (events.size() != 1 || events[0].type != lob::EventType::Accepted) {
      FailWithHistory("reference rejected a non-crossing rest", history);
    }
    live.push_back(order.id);
    RequireAgree("after rest", history, Capture(production, live, kFullDepth),
                 Capture(reference, live, kFullDepth));
  }
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

int main() {
  TestGoldenNonCrossingRest();
  TestBidAndAskPriceOrder();
  TestSamePriceSumsLevel();
  TestSellOnlyBook();
  return 0;
}

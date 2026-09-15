#include <cassert>
#include <vector>

#include "reference_book.hpp"

void TestNonCrossingGtcRests() {
  lob::ref::ReferenceBook book;
  const lob::NewOrder order{1, lob::Side::Buy, 100, 10, lob::TimeInForce::GTC};

  const std::vector<lob::Event> events = book.Submit(order);

  assert(events.size() == 1);
  assert(events[0].type == lob::EventType::Accepted);
  assert(events[0].orderId == 1);
  assert(events[0].reason == lob::RejectReason::None);

  const std::vector<lob::LevelSnapshot> bids = book.Depth(lob::Side::Buy, 10);
  assert(bids.size() == 1);
  assert(bids[0].price == 100);
  assert(bids[0].quantity == 10);
  assert(book.Depth(lob::Side::Sell, 10).empty());
  assert(book.GetRestingQuantity(1) == 10);

  const lob::LevelSnapshot top = book.Top(lob::Side::Buy);
  assert(top.price == 100);
  assert(top.quantity == 10);
}

void TestBidAndAskPriceOrder() {
  lob::ref::ReferenceBook book;
  const lob::NewOrder low_bid{1, lob::Side::Buy, 100, 10, lob::TimeInForce::GTC};
  const lob::NewOrder high_bid{2, lob::Side::Buy, 101, 5, lob::TimeInForce::GTC};
  const lob::NewOrder high_ask{3, lob::Side::Sell, 110, 4, lob::TimeInForce::GTC};
  const lob::NewOrder low_ask{4, lob::Side::Sell, 105, 7, lob::TimeInForce::GTC};

  book.Submit(low_bid);
  book.Submit(high_bid);
  book.Submit(high_ask);
  book.Submit(low_ask);

  const std::vector<lob::LevelSnapshot> bids = book.Depth(lob::Side::Buy, 10);
  assert(bids.size() == 2);
  assert(bids[0].price == 101);
  assert(bids[0].quantity == 5);
  assert(bids[1].price == 100);
  assert(bids[1].quantity == 10);

  const std::vector<lob::LevelSnapshot> asks = book.Depth(lob::Side::Sell, 10);
  assert(asks.size() == 2);
  assert(asks[0].price == 105);
  assert(asks[0].quantity == 7);
  assert(asks[1].price == 110);
  assert(asks[1].quantity == 4);

  const lob::LevelSnapshot best_bid = book.Top(lob::Side::Buy);
  assert(best_bid.price == 101);
  assert(best_bid.quantity == 5);
  const lob::LevelSnapshot best_ask = book.Top(lob::Side::Sell);
  assert(best_ask.price == 105);
  assert(best_ask.quantity == 7);
}

void TestSamePriceSumsLevel() {
  lob::ref::ReferenceBook book;
  const lob::NewOrder first{1, lob::Side::Buy, 100, 10, lob::TimeInForce::GTC};
  const lob::NewOrder second{2, lob::Side::Buy, 100, 5, lob::TimeInForce::GTC};

  book.Submit(first);
  book.Submit(second);

  const std::vector<lob::LevelSnapshot> bids = book.Depth(lob::Side::Buy, 10);
  assert(bids.size() == 1);
  assert(bids[0].price == 100);
  assert(bids[0].quantity == 15);
  const lob::LevelSnapshot top = book.Top(lob::Side::Buy);
  assert(top.price == 100);
  assert(top.quantity == 15);
  assert(book.GetRestingQuantity(1) == 10);
  assert(book.GetRestingQuantity(2) == 5);
}

void TestZeroQuantityRejectsWithoutRest() {
  lob::ref::ReferenceBook book;
  const lob::NewOrder order{1, lob::Side::Buy, 100, 0, lob::TimeInForce::GTC};

  const std::vector<lob::Event> events = book.Submit(order);

  assert(events.size() == 1);
  assert(events[0].type == lob::EventType::Rejected);
  assert(events[0].reason == lob::RejectReason::ZeroQuantity);
  assert(book.Depth(lob::Side::Buy, 10).empty());
  assert(book.GetRestingQuantity(1) == 0);
}

void TestDuplicateIdRejectsWithoutMutation() {
  lob::ref::ReferenceBook book;
  const lob::NewOrder first{1, lob::Side::Buy, 100, 10, lob::TimeInForce::GTC};
  const lob::NewOrder duplicate{1, lob::Side::Buy, 101, 8, lob::TimeInForce::GTC};

  book.Submit(first);
  const std::vector<lob::Event> events = book.Submit(duplicate);

  assert(events.size() == 1);
  assert(events[0].type == lob::EventType::Rejected);
  assert(events[0].reason == lob::RejectReason::DuplicateOrderId);

  const std::vector<lob::LevelSnapshot> bids = book.Depth(lob::Side::Buy, 10);
  assert(bids.size() == 1);
  assert(bids[0].price == 100);
  assert(bids[0].quantity == 10);
  assert(book.GetRestingQuantity(1) == 10);
}

int main() {
  TestNonCrossingGtcRests();
  TestBidAndAskPriceOrder();
  TestSamePriceSumsLevel();
  TestZeroQuantityRejectsWithoutRest();
  TestDuplicateIdRejectsWithoutMutation();
  return 0;
}

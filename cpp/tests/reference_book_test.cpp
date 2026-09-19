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

void TestCrossTradesAtMakerPrice() {
  lob::ref::ReferenceBook book;
  book.Submit({1, lob::Side::Sell, 100, 3, lob::TimeInForce::GTC});
  const std::vector<lob::Event> events =
      book.Submit({2, lob::Side::Buy, 101, 3, lob::TimeInForce::GTC});

  assert(events.size() == 4);
  assert(events[0].type == lob::EventType::Accepted);
  assert(events[0].orderId == 2);
  assert(events[1].type == lob::EventType::Trade);
  assert(events[1].orderId == 1);
  assert(events[1].counterOrderId == 2);
  assert(events[1].price == 100);
  assert(events[1].quantity == 3);
  assert(events[2].type == lob::EventType::Filled);
  assert(events[2].orderId == 1);
  assert(events[3].type == lob::EventType::Filled);
  assert(events[3].orderId == 2);

  assert(book.Depth(lob::Side::Buy, 10).empty());
  assert(book.Depth(lob::Side::Sell, 10).empty());
  assert(book.GetRestingQuantity(1) == 0);
  assert(book.GetRestingQuantity(2) == 0);
}

void TestFifoAtOnePrice() {
  lob::ref::ReferenceBook book;
  book.Submit({1, lob::Side::Sell, 100, 3, lob::TimeInForce::GTC});
  book.Submit({2, lob::Side::Sell, 100, 5, lob::TimeInForce::GTC});
  const std::vector<lob::Event> events =
      book.Submit({3, lob::Side::Buy, 100, 4, lob::TimeInForce::GTC});

  assert(events.size() == 5);
  assert(events[0].type == lob::EventType::Accepted);
  assert(events[0].orderId == 3);
  assert(events[1].type == lob::EventType::Trade);
  assert(events[1].orderId == 1);
  assert(events[1].counterOrderId == 3);
  assert(events[1].price == 100);
  assert(events[1].quantity == 3);
  assert(events[2].type == lob::EventType::Trade);
  assert(events[2].orderId == 2);
  assert(events[2].counterOrderId == 3);
  assert(events[2].price == 100);
  assert(events[2].quantity == 1);
  assert(events[3].type == lob::EventType::Filled);
  assert(events[3].orderId == 1);
  assert(events[4].type == lob::EventType::Filled);
  assert(events[4].orderId == 3);

  const std::vector<lob::LevelSnapshot> asks = book.Depth(lob::Side::Sell, 10);
  assert(asks.size() == 1);
  assert(asks[0].price == 100);
  assert(asks[0].quantity == 4);
  assert(book.GetRestingQuantity(2) == 4);
  assert(book.GetRestingQuantity(1) == 0);
  assert(book.GetRestingQuantity(3) == 0);
}

void TestTakerRemainderRests() {
  lob::ref::ReferenceBook book;
  book.Submit({1, lob::Side::Sell, 100, 3, lob::TimeInForce::GTC});
  const std::vector<lob::Event> events =
      book.Submit({2, lob::Side::Buy, 101, 10, lob::TimeInForce::GTC});

  assert(events.size() == 3);
  assert(events[0].type == lob::EventType::Accepted);
  assert(events[1].type == lob::EventType::Trade);
  assert(events[1].quantity == 3);
  assert(events[1].price == 100);
  assert(events[2].type == lob::EventType::Filled);
  assert(events[2].orderId == 1);

  const std::vector<lob::LevelSnapshot> bids = book.Depth(lob::Side::Buy, 10);
  assert(bids.size() == 1);
  assert(bids[0].price == 101);
  assert(bids[0].quantity == 7);
  assert(book.Depth(lob::Side::Sell, 10).empty());
  assert(book.GetRestingQuantity(2) == 7);
}

void TestSweepStopsAtUnacceptablePrice() {
  lob::ref::ReferenceBook book;
  book.Submit({1, lob::Side::Sell, 100, 3, lob::TimeInForce::GTC});
  book.Submit({2, lob::Side::Sell, 110, 5, lob::TimeInForce::GTC});
  book.Submit({3, lob::Side::Buy, 101, 10, lob::TimeInForce::GTC});

  assert(book.GetRestingQuantity(1) == 0);
  assert(book.GetRestingQuantity(2) == 5);
  assert(book.GetRestingQuantity(3) == 7);
  assert(book.Top(lob::Side::Sell).price == 110);
  assert(book.Top(lob::Side::Buy).price == 101);
}

void TestUnknownCancel() {
  lob::ref::ReferenceBook book;
  const std::vector<lob::Event> events = book.Cancel(9);
  assert(events.size() == 1);
  assert(events[0].type == lob::EventType::Rejected);
  assert(events[0].reason == lob::RejectReason::UnknownOrder);
}

void TestCancelHeadMiddleTail() {
  lob::ref::ReferenceBook book;
  book.Submit({1, lob::Side::Buy, 100, 1, lob::TimeInForce::GTC});
  book.Submit({2, lob::Side::Buy, 100, 2, lob::TimeInForce::GTC});
  book.Submit({3, lob::Side::Buy, 100, 3, lob::TimeInForce::GTC});

  const std::vector<lob::Event> middle = book.Cancel(2);
  assert(middle.size() == 1);
  assert(middle[0].type == lob::EventType::Cancelled);
  assert(middle[0].orderId == 2);
  assert(middle[0].quantity == 2);
  assert(book.GetRestingQuantity(2) == 0);
  assert(book.GetRestingQuantity(1) == 1);
  assert(book.GetRestingQuantity(3) == 3);
  assert(book.Depth(lob::Side::Buy, 10)[0].quantity == 4);

  book.Cancel(1);
  assert(book.GetRestingQuantity(1) == 0);
  assert(book.GetRestingQuantity(3) == 3);
  assert(book.Top(lob::Side::Buy).quantity == 3);

  book.Cancel(3);
  assert(book.Depth(lob::Side::Buy, 10).empty());
  assert(book.Top(lob::Side::Buy).quantity == 0);
}

void TestCancelFilledIsUnknown() {
  lob::ref::ReferenceBook book;
  book.Submit({1, lob::Side::Sell, 100, 3, lob::TimeInForce::GTC});
  book.Submit({2, lob::Side::Buy, 100, 3, lob::TimeInForce::GTC});
  const std::vector<lob::Event> events = book.Cancel(1);
  assert(events[0].type == lob::EventType::Rejected);
  assert(events[0].reason == lob::RejectReason::UnknownOrder);
}

int main() {
  TestNonCrossingGtcRests();
  TestBidAndAskPriceOrder();
  TestSamePriceSumsLevel();
  TestZeroQuantityRejectsWithoutRest();
  TestDuplicateIdRejectsWithoutMutation();
  TestCrossTradesAtMakerPrice();
  TestFifoAtOnePrice();
  TestTakerRemainderRests();
  TestSweepStopsAtUnacceptablePrice();
  TestUnknownCancel();
  TestCancelHeadMiddleTail();
  TestCancelFilledIsUnknown();
  return 0;
}

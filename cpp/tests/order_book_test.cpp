#include <cassert>
#include "lob/order_book.hpp"

void TestEmptyBook() {
  lob::OrderBook book;
  assert(book.GetVolumeAtPriceAndSide(100, lob::Side::Buy) == 0);
}

void TestOneAddedOrder() {
  lob::OrderBook book;
  const lob::Order order = {1, lob::Side::Buy, 100, 100};

  book.AddOrder(order);
  assert(book.GetVolumeAtPriceAndSide(100, lob::Side::Buy) == 100);
}

void TestTwoAddedOrders() {
  lob::OrderBook book;
  const lob::Order order = {1, lob::Side::Buy, 100, 100};
  book.AddOrder(order);
  const lob::Order Order2 = {2, lob::Side::Buy, 100, 50};
  book.AddOrder(Order2);
  assert(book.GetVolumeAtPriceAndSide(100, lob::Side::Buy) == 150);
}

void TestWrongSideExcluded() {
  lob::OrderBook book;
  const lob::Order order = {1, lob::Side::Buy, 100, 100};
  book.AddOrder(order);
  const lob::Order sellOrder = {2, lob::Side::Sell, 100, 100};
  book.AddOrder(sellOrder);
  assert(book.GetVolumeAtPriceAndSide(100, lob::Side::Buy) == 100);
}

void TestWrongPriceExcluded() {
  lob::OrderBook book;
  const lob::Order order = {1, lob::Side::Buy, 100, 100};
  book.AddOrder(order);
  const lob::Order order2 = {2, lob::Side::Buy, 50, 50};
  book.AddOrder(order2);
  assert(book.GetVolumeAtPriceAndSide(100, lob::Side::Buy) == 100);
  assert(book.GetVolumeAtPriceAndSide(50, lob::Side::Buy) == 50);
}

void TestTopIsAccurate() {
  lob::OrderBook book;
  const lob::Order order = {1, lob::Side::Buy, 100, 100};
  book.AddOrder(order);
  const lob::Order smallerOrder = {2, lob::Side::Buy, 90, 100};
  book.AddOrder(smallerOrder);
  lob::LevelSnapshot top = book.Top(lob::Side::Buy);
  assert(top.price == 100);
}

void TestDepthCombinesMultipleOrdersAtSamePrice() {
  lob::OrderBook book;
  const lob::Order order = {1, lob::Side::Buy, 100, 100};
  book.AddOrder(order);
  const lob::Order smallerOrder = {2, lob::Side::Buy, 100, 100};
  book.AddOrder(smallerOrder);
  std::vector<lob::LevelSnapshot> levels = book.Depth(lob::Side::Buy, 2);
  assert(levels[0].price == 100);
  assert(levels[0].quantity == 200);
}

void TestGetRestingQuantityFindsRightOrder() {
  lob::OrderBook book;
  const lob::Order order = {1, lob::Side::Buy, 100, 100};
  book.AddOrder(order);
  const lob::Order smallerOrder = {2, lob::Side::Buy, 90, 90};
  book.AddOrder(smallerOrder);
  assert(book.GetRestingQuantity(1) == 100);
  assert(book.GetRestingQuantity(2) == 90);
}

void TestSubmit() {
  lob::OrderBook book;
  const lob::NewOrder order = {1, lob::Side::Buy, 100, 100, lob::TimeInForce::GTC};
  std::vector<lob::Event> first_events = book.Submit(order);
  assert(first_events.size() == 1);
  assert(first_events[0].type == lob::EventType::Accepted);
  // submit order that should be cancelled
  const lob::NewOrder invalid = {1, lob::Side::Sell, 100, 100, lob::TimeInForce::GTC};
  std::vector<lob::Event> invalid_event = book.Submit(invalid);
  assert(invalid_event[0].type == lob::EventType::Rejected);
  assert(invalid_event[0].reason == lob::RejectReason::DuplicateOrderId);

  const lob::NewOrder invalid_2 = {2, lob::Side::Sell, 100, 0, lob::TimeInForce::GTC};
  std::vector<lob::Event> invalid_event_2 = book.Submit(invalid_2);
  assert(invalid_event_2[0].reason == lob::RejectReason::ZeroQuantity);
  assert(invalid_event_2[0].type == lob::EventType::Rejected);

  const lob::NewOrder sell = {2, lob::Side::Sell, 100, 100, lob::TimeInForce::GTC};
  std::vector<lob::Event> filled = book.Submit(sell);
  // Accepted, Trade, Filled, Filled
  assert(filled.size() == 4);
  assert(book.GetRestingQuantity(1) == 0);
  assert(book.GetRestingQuantity(2) == 0);

  assert(book.Depth(lob::Side::Buy, 1).size() == 0);
  assert(book.Depth(lob::Side::Sell, 1).size() == 0);

}

void TestSubmitGolden3() {
  lob::OrderBook book;
  const lob::NewOrder order = {1, lob::Side::Sell, 100, 3, lob::TimeInForce::GTC};
  const lob::NewOrder order2 = {2, lob::Side::Sell, 100, 5, lob::TimeInForce::GTC};
  std::vector<lob::Event> sell1 = book.Submit(order);
  std::vector<lob::Event> sell2 = book.Submit(order2);

  const lob::NewOrder buy = {3, lob::Side::Buy, 100, 4, lob::TimeInForce::GTC};
  std::vector<lob::Event> buy1 = book.Submit(buy);
  assert(buy1.size() == 5);
  assert(buy1[0].type == lob::EventType::Accepted);
  assert(buy1[1].type == lob::EventType::Trade);
  assert(buy1[2].type == lob::EventType::Filled);
  assert(buy1[3].type == lob::EventType::Trade);
  assert(buy1[4].type == lob::EventType::Filled);
  assert(book.GetRestingQuantity(1) == 0);
  assert(book.GetRestingQuantity(2) == 4);
}

void TestCancel() {
  lob::OrderBook book;
  // Testing unknown id failing cancel
  std::vector<lob::Event> failed_cancel = book.Cancel(0);
  assert(failed_cancel[0].type == lob::EventType::Rejected);
  assert(failed_cancel[0].reason == lob::RejectReason::UnknownOrder);

  // we'll reuse the following orders
  const lob::NewOrder order = {1, lob::Side::Buy, 100, 3, lob::TimeInForce::GTC};
  const lob::NewOrder order2 = {2, lob::Side::Buy, 100, 4, lob::TimeInForce::GTC};
  const lob::NewOrder order3 = {3, lob::Side::Buy, 100, 5, lob::TimeInForce::GTC};

  // cancelling one works fine
  book.Submit(order);
  std::vector<lob::Event> cancel_success = book.Cancel(1);
  assert(book.GetRestingQuantity(1) == 0);
  assert(cancel_success[0].type == lob::EventType::Cancelled);
  assert(book.GetVolumeAtPriceAndSide(100, lob::Side::Buy) == 0);

  // cancelling mid and seeing head and tail are fine and in fifo order
  book.Submit(order);
  book.Submit(order2);
  book.Submit(order3);

  std::vector<lob::Event> mid = book.Cancel(2);
  assert(mid[0].type == lob::EventType::Cancelled);
  assert(book.GetRestingQuantity(1) == 3);
  assert(book.GetRestingQuantity(3) == 5);
  assert(book.Top(lob::Side::Buy).quantity == 8);


  // proving FIFO order by adding a sell
  const lob::NewOrder sell = {4, lob::Side::Sell, 100, 1, lob::TimeInForce::GTC};
  std::vector<lob::Event> sell_events = book.Submit(sell);
  assert(sell_events[1].type == lob::EventType::Trade);
  assert(sell_events[1].counterOrderId == 1);

  assert(book.GetRestingQuantity(1) == 2);
  assert(book.GetRestingQuantity(3) == 5);
  assert(book.Top(lob::Side::Buy).quantity == 7);

  // Cancel head
  std::vector<lob::Event> head = book.Cancel(1);
  assert(book.GetRestingQuantity(3) == 5);
  assert(book.Top(lob::Side::Buy).quantity == 5);

  // cancel tail
  std::vector<lob::Event> tail = book.Cancel(3);
  assert(book.Top(lob::Side::Buy).quantity == 0);
}

int main() {
  TestEmptyBook();
  TestOneAddedOrder();
  TestTwoAddedOrders();
  TestWrongPriceExcluded();
  TestWrongSideExcluded();
  TestTopIsAccurate();
  TestDepthCombinesMultipleOrdersAtSamePrice();
  TestGetRestingQuantityFindsRightOrder();
  TestSubmit();
  TestSubmitGolden3();
  TestCancel();
  return 0;
}
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

int main() {
  TestEmptyBook();
  TestOneAddedOrder();
  TestTwoAddedOrders();
  TestWrongPriceExcluded();
  TestWrongSideExcluded();
  TestTopIsAccurate();
  TestDepthCombinesMultipleOrdersAtSamePrice();
  TestGetRestingQuantityFindsRightOrder();

  return 0;
}
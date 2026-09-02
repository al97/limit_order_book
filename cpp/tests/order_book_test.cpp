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

int main() {
  TestEmptyBook();
  TestOneAddedOrder();
  TestTwoAddedOrders();
  TestWrongPriceExcluded();
  TestWrongSideExcluded();

  return 0;
}
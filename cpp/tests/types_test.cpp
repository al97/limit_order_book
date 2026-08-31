#include <cassert>

#include "lob/types.hpp"

int main() {
  const lob::Order order = { 1, lob::Side::Buy, 100, 100 };
  assert(order.id == 1);
  assert(order.side == lob::Side::Buy);
  assert(order.quantity == 100);
  assert(order.price == 100);

  return 0;
}
#include "lob/types.hpp"
#include "lob/order_book.hpp"

namespace lob {

void OrderBook::AddOrder(const Order& order) {
  orders.push_back(order);
}
Quantity OrderBook::GetVolumeAtPriceAndSide(
  PriceTicks price, Side side) const {
  Quantity quantity = 0;
  for (const Order& order : orders) {
    if (price == order.price && side == order.side) {
      quantity += order.quantity;
    }
  }
  return quantity;
}

} //namespace lob
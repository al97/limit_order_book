#include "lob/order_book.hpp"

namespace lob {

std::vector<Event> OrderBook::Submit(const NewOrder& order) {
  std::vector<Event> events;
  // Validate
  if (order.quantity <= 0) {
    Event event = Event{.sequence = next_seq(),
                        .type = EventType::Rejected,
                        .orderId = order.id,
                        .counterOrderId = 0,
                        .price = order.price,
                        .quantity = order.quantity,
                        .reason = RejectReason::ZeroQuantity};
    events.push_back(event);
    return events;
  }
  
  // duplicate order id 
  const auto it = locator.find(order.id);
  if (it != locator.end()) {
    Event event = Event{.sequence = next_seq(),
                        .type = EventType::Rejected,
                        .orderId = order.id,
                        .counterOrderId = 0,
                        .price = order.price,
                        .quantity = order.quantity,
                        .reason = RejectReason::DuplicateOrderId};
    events.push_back(event);
    return events;
  }

  // Match against opposite side
  if (order.side == Side::Buy) {

    while (order.quantity > 0 && Top(Side::Sell).price <= order.price) {
      // take fromt front of best ask queue
      const auto& [price, queue] = *sellSideMap.begin();
      if (order.quantity == 0) { break; }
      for (auto opp : queue.begin()) {
        if (order.quantity == 0) { break; }
        // exhaust quantity of either side
        if (opp.quantity >= order.quantity) {
          // order has been filled, emit and break
          Event event = Event{.sequence = next_seq(),
                              .type = EventType::Filled,
                              .orderId = order.id,
                              .counterOrderId = opp.id,
                              .price = opp.price,
                              .quantity = order.quantity,
                              .reason = RejectReason::None};
          events.push_back(event);
          return events;
        } else {
          // order has not been filled but an existing one has.
          Event event = Event{.sequence = next_seq(),
                              .type = EventType::Filled,
                              .orderId = opp.id,
                              .counterOrderId = order.id,
                              .price = opp.price,
                              .quantity = order.quantity,
                              .reason = RejectReason::None};
          events.push_back(event);
          opp = queue.erase(opp);
          order.quantity -= opp.quantity;
        }
      }
    }
  }

  // Rest remainder GTC
  // Return events
  return events;

}
std::vector<Event> OrderBook::Cancel(OrderId id) {



}

void OrderBook::AddOrder(const Order& order) {
  if (order.side == Side::Buy) {
    auto& queue = buySideMap[order.price];
    queue.push_back(order);
    locator[order.id] = std::prev(queue.end());
  }
  else {
    auto& queue = sellSideMap[order.price];
    queue.push_back(order);
    locator[order.id] = std::prev(queue.end());
  }
}

Quantity OrderBook::GetVolumeAtPriceAndSide(
  PriceTicks price, Side side) const {
  Quantity quantity = 0;
  if (side == Side::Buy) {
    const auto it = buySideMap.find(price);
    if (it == buySideMap.end()) {
      return 0;
    }
    for (const Order& order : it->second) {
      quantity += order.quantity;
    }
  } else {
    const auto it = sellSideMap.find(price);
    if (it == sellSideMap.end()) {
      return 0;
    }
    for (const Order& order : it->second) {
      quantity += order.quantity;
    }
  }
  return quantity;
}

Quantity OrderBook::GetRestingQuantity(OrderId id) const {
  const auto it = locator.find(id);
  if (it == locator.end()) {
    return 0;
  }
  
  return (*it->second).quantity;
}

LevelSnapshot OrderBook::Top(Side side) {
  LevelSnapshot level;
  if (side == Side::Buy && !buySideMap.empty()) {
    const auto& [price, queue] = *buySideMap.begin();
    level.price = price;
    for (const Order& order : queue) {
      level.quantity += order.quantity;
    }
  }
  else if (side == Side::Sell && !sellSideMap.empty()) {
    const auto& [price, queue] = *sellSideMap.begin();
    level.price = price;
    for (const Order& order : queue) {
      level.quantity += order.quantity;
    }
  }
  return level;
}

std::vector<LevelSnapshot> OrderBook::Depth(Side side, std::size_t levels) const {
  std::vector<LevelSnapshot> level_snap;
  if (side == Side::Buy) {
    for (const auto& [price, order_queue] : buySideMap) {
      LevelSnapshot snap_at_price;
      snap_at_price.price = price;
      for (const auto& order : order_queue) {
        snap_at_price.quantity += order.quantity;
      }
      level_snap.push_back(snap_at_price);
      if (level_snap.size() == levels) {
        break;
      }
    }
  }
  else {
    for (const auto& [price, order_queue] : sellSideMap) {
      LevelSnapshot snap_at_price;
      snap_at_price.price = price;
      for (const auto& order : order_queue) {
        snap_at_price.quantity += order.quantity;
      }
      level_snap.push_back(snap_at_price);
      if (level_snap.size() == levels) {
        break;
      }
    }
  }
  return level_snap;
}

} //namespace lob
#include "lob/order_book.hpp"
#include <limits>

#include <limits>

namespace lob {
namespace {

Quantity SaturatingAdd(Quantity left, Quantity right) {
  if (right > std::numeric_limits<Quantity>::max() - left) {
    return std::numeric_limits<Quantity>::max();
  }
  return left + right;
}

bool Compatible(const NewOrder& order, PriceTicks maker_price) {
  if (order.tif == TimeInForce::Market) {
    return true;
  }
  if (order.side == Side::Buy) {
    return maker_price <= order.price;
  }
  return maker_price >= order.price;
}

template <class Levels>
Quantity AvailableAgainst(const Levels& levels, const NewOrder& taker) {
  Quantity available = 0;
  for (const auto& [price, queue] : levels) {
    if (!Compatible(taker, price)) {
      break;
    }
    for (const Order& resting : queue) {
      available = SaturatingAdd(available, resting.quantity);
      if (available >= taker.quantity) {
        return available;
      }
    }
  }
  return available;
}

}  // namespace

Event OrderBook::Emit(EventType type, OrderId order_id, OrderId counter_id,
  PriceTicks price, Quantity quantity, RejectReason reason) {
  return Event{.sequence = next_seq(), .type = type, .orderId = order_id,
               .counterOrderId = counter_id, .price = price,
               .quantity = quantity, .reason = reason};
}

std::vector<Event> OrderBook::Submit(const NewOrder& order) {
  std::vector<Event> events;
  // Validate
  if (order.quantity <= 0) {
    events.push_back(Emit(EventType::Rejected, order.id,
                          0, order.price, order.quantity, RejectReason::ZeroQuantity));
    return events;
  }
  
  // duplicate order id 
  const auto it = locator.find(order.id);
  if (it != locator.end()) {
    events.push_back(Emit(EventType::Rejected, order.id,
                          0, order.price, order.quantity, RejectReason::DuplicateOrderId));
    return events;
  }

  if (order.tif == TimeInForce::FOK) {
    const Quantity available = order.side == Side::Buy
                                   ? AvailableAgainst(sellSideMap, order)
                                   : AvailableAgainst(buySideMap, order);
    if (available < order.quantity) {
      events.push_back(Emit(EventType::Rejected, order.id, 0, order.price,
                            order.quantity, RejectReason::None));
      return events;
    }
  }

  // Accept the order!
  events.push_back(Emit(EventType::Accepted, order.id, 0, order.price, order.quantity));

  // Match against opposite side
  if (order.side == Side::Buy) {
    Quantity remaining = order.quantity;
    while (remaining > 0 && !sellSideMap.empty() &&
           Compatible(order, Top(Side::Sell).price)) {
      // take fromt front of best ask queue in each iteration
      auto it = sellSideMap.begin();
      PriceTicks price = it->first;
      auto& queue = it->second;
      auto opp_it = queue.begin();
      Order& opp = *opp_it;
      if (remaining == 0) { break; }
      // exhaust quantity of either side
      if (opp.quantity > remaining) {
        // order has been filled, emit and break
        events.push_back(Emit(EventType::Trade, order.id,
                              opp.id, opp.price, remaining));
        events.push_back(Emit(EventType::Filled, order.id,
                              opp.id, opp.price, remaining));
        opp.quantity -= remaining;
        remaining = 0;
      } else if (opp.quantity < remaining) {
        // order has not been filled but an existing one has.
        events.push_back(Emit(EventType::Trade, order.id,
                              opp.id, opp.price, opp.quantity));
        events.push_back(Emit(EventType::Filled, opp.id,
                              order.id, opp.price, opp.quantity));
        remaining -= opp.quantity;
        locator.erase(opp.id);
        opp_it = queue.erase(opp_it);
      } else {
        // both have been filled
        events.push_back(Emit(EventType::Trade, order.id,
                  opp.id, opp.price, opp.quantity));
        events.push_back(Emit(EventType::Filled, opp.id,
                  order.id, opp.price, remaining));
        events.push_back(Emit(EventType::Filled, order.id,
                  opp.id, opp.price, opp.quantity));
        remaining -= opp.quantity;
        locator.erase(opp.id);
        opp_it = queue.erase(opp_it);
      }
      if (queue.empty()) {
        sellSideMap.erase(price);
      }
    }

    if (remaining > 0 && order.tif == TimeInForce::GTC) {
      AddOrder({order.id, order.side, order.price, remaining});
    } else if (remaining > 0 && (order.tif == TimeInForce::IOC ||
                                 order.tif == TimeInForce::Market)) {
      events.push_back(Emit(EventType::Cancelled, order.id, 0, order.price,
                            remaining));
    }
  } else {
    Quantity remaining = order.quantity;
    while (remaining > 0 && !buySideMap.empty() &&
           Compatible(order, Top(Side::Buy).price)) {
      // take fromt front of best bid queue in each iteration
      auto it = buySideMap.begin();
      PriceTicks price = it->first;
      auto& queue = it->second;
      auto opp_it = queue.begin();
      Order& opp = *opp_it;
      if (remaining == 0) { break; }
      // exhaust quantity of either side
      if (opp.quantity > remaining) {
        // order has been filled, emit and break
        events.push_back(Emit(EventType::Trade, order.id,
                              opp.id, opp.price, remaining));
        events.push_back(Emit(EventType::Filled, order.id,
                              opp.id, opp.price, remaining));
        opp.quantity -= remaining;
        remaining = 0;
      } else if (opp.quantity < remaining) {
        // order has not been filled but an existing one has.
        events.push_back(Emit(EventType::Trade, order.id,
                              opp.id, opp.price, opp.quantity));
        events.push_back(Emit(EventType::Filled, opp.id,
                              order.id, opp.price, opp.quantity));
        remaining -= opp.quantity;
        locator.erase(opp.id);
        opp_it = queue.erase(opp_it);
      } else {
        // both have been filled
        events.push_back(Emit(EventType::Trade, order.id,
                  opp.id, opp.price, opp.quantity));
        events.push_back(Emit(EventType::Filled, opp.id,
                  order.id, opp.price, remaining));
        events.push_back(Emit(EventType::Filled, order.id,
                  opp.id, opp.price, opp.quantity));
        remaining -= opp.quantity;
        locator.erase(opp.id);
        opp_it = queue.erase(opp_it);
      }
      if (queue.empty()) {
        buySideMap.erase(price);
      }
    }

    if (remaining > 0 && order.tif == TimeInForce::GTC) {
      AddOrder({order.id, order.side, order.price, remaining});
    } else if (remaining > 0 && (order.tif == TimeInForce::IOC ||
                                 order.tif == TimeInForce::Market)) {
      events.push_back(Emit(EventType::Cancelled, order.id, 0, order.price,
                            remaining));
    }
  }

  // Return events
  return events;

}

std::vector<Event> OrderBook::Cancel(OrderId id) {
  // Find the order
  std::vector<Event> events;
  const auto it = locator.find(id);
  if (it == locator.end()) {
    events.push_back(Emit(EventType::Rejected, id,
                          0, 0, 0, RejectReason::UnknownOrder));
    return events;
  }
  // we nknow the iterator exists now. so we just need to grab the list itself:
  // grab the order itself, and get the price and side.
  auto order = *it->second;
  events.push_back(Emit(EventType::Cancelled, id,
    0, order.price, order.quantity));
  if (order.side == Side::Sell) {
    auto& queue = sellSideMap[order.price];
    queue.erase(it->second);
    locator.erase(id);
    if (queue.empty()) {
      sellSideMap.erase(order.price);
    }
  }
  else if (order.side == Side::Buy) {
    auto& queue = buySideMap[order.price];
    queue.erase(it->second);
    locator.erase(id);
    if (queue.empty()) {
      buySideMap.erase(order.price);
    }
  }
  return events;
}

Event OrderBook::Emit(EventType type, OrderId order_id, OrderId counter_id,
  PriceTicks price, Quantity quantity, RejectReason reason) {
  return Event{.sequence = next_seq(), .type = type, .orderId = order_id,
               .counterOrderId = counter_id, .price = price,
               .quantity = quantity, .reason = reason};
}

std::vector<Event> OrderBook::Submit(const NewOrder& order) {
  std::vector<Event> events;
  // Validate
  if (order.quantity <= 0) {
    events.push_back(Emit(EventType::Rejected, order.id,
                          0, order.price, order.quantity, RejectReason::ZeroQuantity));
    return events;
  }
  
  // duplicate order id 
  const auto it = locator.find(order.id);
  if (it != locator.end()) {
    events.push_back(Emit(EventType::Rejected, order.id,
                          0, order.price, order.quantity, RejectReason::DuplicateOrderId));
    return events;
  }
  // Before accepting for FOK - need to find out if we have enough to FILL.
  if (order.tif == TimeInForce::FOK) {
    if (order.side == Side::Buy) {
      Quantity available = 0;
      auto it = sellSideMap.begin();
      while (available < order.quantity && it != sellSideMap.end() && it->first <= order.price) {
        auto& queue = it->second;
        auto opp_it = queue.begin();
        // now walk through the queue
        while (available < order.quantity && opp_it != queue.end()) {
          Order& opp = *opp_it;
          if (__builtin_add_overflow(available, opp.quantity, &available)) {
            available = order.quantity;
            break;
          }
          ++opp_it;
        }
        ++it;
      }
      if (available < order.quantity) {
        events.push_back(Emit(EventType::Rejected, order.id,
                              0, order.price, order.quantity, RejectReason::NotEnoughQuantity));
        return events;
      }
    } else {
      Quantity available = 0;
      auto it = buySideMap.begin();
      while (available < order.quantity && it != buySideMap.end() && it->first >= order.price) {
        auto& queue = it->second;
        auto opp_it = queue.begin();
        // now walk through the queue
        while (available < order.quantity && opp_it != queue.end()) {
          Order& opp = *opp_it;
          if (__builtin_add_overflow(available, opp.quantity, &available)) {
            available = order.quantity;
            break;
          }
          ++opp_it;
        }
        ++it;
      }
      if (available < order.quantity) {
        events.push_back(Emit(EventType::Rejected, order.id,
                              0, order.price, order.quantity, RejectReason::NotEnoughQuantity));
        return events;
      }
    }
  }

  // Accept the order!
  events.push_back(Emit(EventType::Accepted, order.id, 0, order.price, order.quantity));

  // Match against opposite side
  PriceTicks limit = order.price;
  if (order.tif == TimeInForce::Market) {
    limit = (order.side == Side::Buy) ? std::numeric_limits<PriceTicks>::max() :
                                        std::numeric_limits<PriceTicks>::min();
  }
  if (order.side == Side::Buy) {
    Quantity remaining = order.quantity;
    while (remaining > 0 && !sellSideMap.empty() && Top(Side::Sell).price <= limit) {
      // take fromt front of best ask queue in each iteration
      auto it = sellSideMap.begin();
      PriceTicks price = it->first;
      auto& queue = it->second;
      auto opp_it = queue.begin();
      Order& opp = *opp_it;
      if (remaining == 0) { break; }
      // exhaust quantity of either side
      if (opp.quantity > remaining) {
        // order has been filled, emit and break
        events.push_back(Emit(EventType::Trade, order.id,
                              opp.id, opp.price, remaining));
        events.push_back(Emit(EventType::Filled, order.id,
                              opp.id, opp.price, remaining));
        opp.quantity -= remaining;
        remaining = 0;
      } else if (opp.quantity < remaining) {
        // order has not been filled but an existing one has.
        events.push_back(Emit(EventType::Trade, order.id,
                              opp.id, opp.price, opp.quantity));
        events.push_back(Emit(EventType::Filled, opp.id,
                              order.id, opp.price, opp.quantity));
        remaining -= opp.quantity;
        locator.erase(opp.id);
        opp_it = queue.erase(opp_it);
      } else {
        // both have been filled
        events.push_back(Emit(EventType::Trade, order.id,
                  opp.id, opp.price, opp.quantity));
        events.push_back(Emit(EventType::Filled, opp.id,
                  order.id, opp.price, remaining));
        events.push_back(Emit(EventType::Filled, order.id,
                  opp.id, opp.price, opp.quantity));
        remaining -= opp.quantity;
        locator.erase(opp.id);
        opp_it = queue.erase(opp_it);
      }
      if (queue.empty()) {
        sellSideMap.erase(price);
      }
    }

    // Rest GTC
    if (remaining > 0 && order.tif == TimeInForce::GTC) {
      AddOrder({order.id, order.side, order.price, remaining});
    } else if (remaining > 0 && (order.tif == TimeInForce::IOC || order.tif == TimeInForce::Market)) {
      events.push_back(Emit(EventType::Cancelled, order.id, 0, order.price, remaining));
    }
  } else {
    Quantity remaining = order.quantity;
    while (remaining > 0 && !buySideMap.empty() && Top(Side::Buy).price >= limit) {
      // take fromt front of best bid queue in each iteration
      auto it = buySideMap.begin();
      PriceTicks price = it->first;
      auto& queue = it->second;
      auto opp_it = queue.begin();
      Order& opp = *opp_it;
      if (remaining == 0) { break; }
      // exhaust quantity of either side
      if (opp.quantity > remaining) {
        // order has been filled, emit and break
        events.push_back(Emit(EventType::Trade, order.id,
                              opp.id, opp.price, remaining));
        events.push_back(Emit(EventType::Filled, order.id,
                              opp.id, opp.price, remaining));
        opp.quantity -= remaining;
        remaining = 0;
      } else if (opp.quantity < remaining) {
        // order has not been filled but an existing one has.
        events.push_back(Emit(EventType::Trade, order.id,
                              opp.id, opp.price, opp.quantity));
        events.push_back(Emit(EventType::Filled, opp.id,
                              order.id, opp.price, opp.quantity));
        remaining -= opp.quantity;
        locator.erase(opp.id);
        opp_it = queue.erase(opp_it);
      } else {
        // both have been filled
        events.push_back(Emit(EventType::Trade, order.id,
                  opp.id, opp.price, opp.quantity));
        events.push_back(Emit(EventType::Filled, opp.id,
                  order.id, opp.price, remaining));
        events.push_back(Emit(EventType::Filled, order.id,
                  opp.id, opp.price, opp.quantity));
        remaining -= opp.quantity;
        locator.erase(opp.id);
        opp_it = queue.erase(opp_it);
      }
      if (queue.empty()) {
        buySideMap.erase(price);
      }
    }

    // Rest GTC
    if (remaining > 0 && order.tif == TimeInForce::GTC) {
      AddOrder({order.id, order.side, order.price, remaining});
    } else if (remaining > 0 && (order.tif == TimeInForce::IOC || order.tif == TimeInForce::Market)) {
      events.push_back(Emit(EventType::Cancelled, order.id, 0, order.price, remaining));
    } 

  }

  // Return events
  return events;

}

std::vector<Event> OrderBook::Cancel(OrderId id) {
  // Find the order
  std::vector<Event> events;
  const auto it = locator.find(id);
  if (it == locator.end()) {
    events.push_back(Emit(EventType::Rejected, id,
                          0, 0, 0, RejectReason::UnknownOrder));
    return events;
  }
  // we nknow the iterator exists now. so we just need to grab the list itself:
  // grab the order itself, and get the price and side.
  auto order = *it->second;
  events.push_back(Emit(EventType::Cancelled, id,
    0, order.price, order.quantity));
  if (order.side == Side::Sell) {
    auto& queue = sellSideMap[order.price];
    queue.erase(it->second);
    locator.erase(id);
    if (queue.empty()) {
      sellSideMap.erase(order.price);
    }
  }
  else if (order.side == Side::Buy) {
    auto& queue = buySideMap[order.price];
    queue.erase(it->second);
    locator.erase(id);
    if (queue.empty()) {
      buySideMap.erase(order.price);
    }
  }
  return events;
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
  LevelSnapshot level{.price = 0, .quantity = 0};
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
      LevelSnapshot snap_at_price{.price = price, .quantity = 0};
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
      LevelSnapshot snap_at_price{.price = price, .quantity = 0};
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

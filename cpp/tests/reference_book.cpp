#include "reference_book.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace lob::ref {
namespace {

Quantity SaturatingAdd(Quantity left, Quantity right) {
  if (right > std::numeric_limits<Quantity>::max() - left) {
    return std::numeric_limits<Quantity>::max();
  }
  return left + right;
}

Quantity SumQuantity(const std::deque<Order>& orders) {
  Quantity quantity = 0;
  for (const Order& order : orders) {
    quantity = SaturatingAdd(quantity, order.quantity);
  }
  return quantity;
}

template <class Levels>
std::vector<LevelSnapshot> TakeDepth(const Levels& levels, std::size_t count) {
  std::vector<LevelSnapshot> depth;
  for (const auto& [price, orders] : levels) {
    if (depth.size() == count) {
      break;
    }
    depth.push_back(LevelSnapshot{price, SumQuantity(orders)});
  }
  return depth;
}

template <class Levels>
const Order* FindOnLevel(const Levels& levels, PriceTicks price, OrderId id) {
  const auto level = levels.find(price);
  if (level == levels.end()) {
    return nullptr;
  }
  for (const Order& order : level->second) {
    if (order.id == id) {
      return &order;
    }
  }
  return nullptr;
}

template <class Levels>
Quantity EraseOnLevel(Levels& levels, PriceTicks price, OrderId id) {
  const auto level = levels.find(price);
  if (level == levels.end()) {
    return 0;
  }
  auto& queue = level->second;
  for (auto it = queue.begin(); it != queue.end(); ++it) {
    if (it->id == id) {
      const Quantity quantity = it->quantity;
      queue.erase(it);
      if (queue.empty()) {
        levels.erase(level);
      }
      return quantity;
    }
  }
  return 0;
}

}  // namespace

Event ReferenceBook::Next(EventType type, OrderId id, OrderId counter,
                          PriceTicks price, Quantity quantity,
                          RejectReason reason) {
  return Event{
      .sequence = next_sequence_++,
      .type = type,
      .orderId = id,
      .counterOrderId = counter,
      .price = price,
      .quantity = quantity,
      .reason = reason,
  };
}

bool ReferenceBook::Compatible(Side taker_side, PriceTicks taker_price,
                               PriceTicks maker_price) const {
  if (taker_side == Side::Buy) {
    return maker_price <= taker_price;
  }
  return maker_price >= taker_price;
}

template <class Levels>
Quantity ReferenceBook::AvailableAgainst(const Levels& levels,
                                         const NewOrder& taker) const {
  Quantity available = 0;
  for (const auto& [price, queue] : levels) {
    if (!Compatible(taker.side, taker.price, price)) {
      break;
    }
    for (const Order& order : queue) {
      available = SaturatingAdd(available, order.quantity);
      if (available >= taker.quantity) {
        return available;
      }
    }
  }
  return available;
}

void ReferenceBook::Rest(const NewOrder& order, Quantity remaining) {
  const Order resting{order.id, order.side, order.price, remaining};
  if (order.side == Side::Buy) {
    bids_[order.price].push_back(resting);
  } else {
    asks_[order.price].push_back(resting);
  }
  live_.emplace(order.id, std::pair<Side, PriceTicks>{order.side, order.price});
}

template <class Levels>
Quantity ReferenceBook::MatchAgainst(Levels& levels, const NewOrder& taker,
                                     Quantity remaining,
                                     std::vector<Event>& events) {
  std::vector<OrderId> filled_makers;
  while (remaining > 0 && !levels.empty()) {
    auto level = levels.begin();
    if (taker.tif != TimeInForce::Market &&
        !Compatible(taker.side, taker.price, level->first)) {
      break;
    }
    auto& queue = level->second;
    while (remaining > 0 && !queue.empty()) {
      Order& maker = queue.front();
      const Quantity fill = std::min(remaining, maker.quantity);
      events.push_back(Next(EventType::Trade, maker.id, taker.id, maker.price,
                            fill, RejectReason::None));
      maker.quantity -= fill;
      remaining -= fill;
      if (maker.quantity == 0) {
        filled_makers.push_back(maker.id);
        live_.erase(maker.id);
        queue.pop_front();
      }
    }
    if (queue.empty()) {
      levels.erase(level);
    }
  }
  for (const OrderId id : filled_makers) {
    events.push_back(Next(EventType::Filled, id, 0, 0, 0, RejectReason::None));
  }
  return remaining;
}

std::vector<Event> ReferenceBook::Submit(const NewOrder& order) {
  if (order.quantity == 0) {
    return {Next(EventType::Rejected, order.id, 0, order.price, order.quantity,
                 RejectReason::ZeroQuantity)};
  }
  if (live_.contains(order.id)) {
    return {Next(EventType::Rejected, order.id, 0, order.price, order.quantity,
                 RejectReason::DuplicateOrderId)};
  }

  if (order.tif == TimeInForce::FOK) {
    const Quantity available = order.side == Side::Buy
                                   ? AvailableAgainst(asks_, order)
                                   : AvailableAgainst(bids_, order);
    if (available < order.quantity) {
      return {Next(EventType::Rejected, order.id, 0, order.price, order.quantity,
                   RejectReason::NotEnoughQuantity)};
    }
  }

  std::vector<Event> events;
  events.push_back(Next(EventType::Accepted, order.id, 0, order.price,
                        order.quantity, RejectReason::None));

  Quantity remaining = order.quantity;
  if (order.side == Side::Buy) {
    remaining = MatchAgainst(asks_, order, remaining, events);
  } else {
    remaining = MatchAgainst(bids_, order, remaining, events);
  }

  if (remaining == 0) {
    events.push_back(
        Next(EventType::Filled, order.id, 0, 0, 0, RejectReason::None));
    return events;
  }

  switch (order.tif) {
    case TimeInForce::GTC:
      Rest(order, remaining);
      break;
    case TimeInForce::IOC:
    case TimeInForce::Market:
      events.push_back(Next(EventType::Cancelled, order.id, 0, order.price,
                            remaining, RejectReason::None));
      break;
    case TimeInForce::FOK:
      break;
  }
  return events;
}

std::vector<Event> ReferenceBook::Cancel(OrderId id) {
  const auto found = live_.find(id);
  if (found == live_.end()) {
    return {Next(EventType::Rejected, id, 0, 0, 0, RejectReason::UnknownOrder)};
  }

  const auto [side, price] = found->second;
  const Quantity quantity = side == Side::Buy ? EraseOnLevel(bids_, price, id)
                                              : EraseOnLevel(asks_, price, id);
  live_.erase(found);
  return {Next(EventType::Cancelled, id, 0, price, quantity,
               RejectReason::None)};
}

LevelSnapshot ReferenceBook::Top(Side side) const {
  const std::vector<LevelSnapshot> top = Depth(side, 1);
  if (top.empty()) {
    return LevelSnapshot{0, 0};
  }
  return top.front();
}

std::vector<LevelSnapshot> ReferenceBook::Depth(Side side,
                                                std::size_t levels) const {
  if (side == Side::Buy) {
    return TakeDepth(bids_, levels);
  }
  return TakeDepth(asks_, levels);
}

Quantity ReferenceBook::GetRestingQuantity(OrderId id) const {
  const auto found = live_.find(id);
  if (found == live_.end()) {
    return 0;
  }
  const auto [side, price] = found->second;
  const Order* order = side == Side::Buy ? FindOnLevel(bids_, price, id)
                                         : FindOnLevel(asks_, price, id);
  if (order == nullptr) {
    return 0;
  }
  return order->quantity;
}

}  // namespace lob::ref

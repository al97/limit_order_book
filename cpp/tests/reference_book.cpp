#include "reference_book.hpp"

namespace lob::ref {
namespace {

Quantity SumQuantity(const std::deque<Order>& orders) {
  Quantity quantity = 0;
  for (const Order& order : orders) {
    quantity += order.quantity;
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

}  // namespace

Event ReferenceBook::MakeEvent(EventType type, const NewOrder& order,
                               RejectReason reason) {
  return Event{
      .sequence = next_sequence_++,
      .type = type,
      .orderId = order.id,
      .counterOrderId = 0,
      .price = order.price,
      .quantity = order.quantity,
      .reason = reason,
  };
}

std::vector<Event> ReferenceBook::Submit(const NewOrder& order) {
  if (order.quantity == 0) {
    return {MakeEvent(EventType::Rejected, order, RejectReason::ZeroQuantity)};
  }
  if (live_.contains(order.id)) {
    return {MakeEvent(EventType::Rejected, order, RejectReason::DuplicateOrderId)};
  }

  const Order resting{order.id, order.side, order.price, order.quantity};
  if (order.side == Side::Buy) {
    bids_[order.price].push_back(resting);
  } else {
    asks_[order.price].push_back(resting);
  }
  live_.emplace(order.id, std::pair<Side, PriceTicks>{order.side, order.price});

  return {MakeEvent(EventType::Accepted, order, RejectReason::None)};
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
  const std::deque<Order>& orders =
      side == Side::Buy ? bids_.at(price) : asks_.at(price);
  for (const Order& order : orders) {
    if (order.id == id) {
      return order.quantity;
    }
  }
  return 0;
}

}  // namespace lob::ref

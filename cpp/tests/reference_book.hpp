#pragma once

#include "lob/types.hpp"

#include <cstddef>
#include <deque>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lob::ref {

class ReferenceBook {
 public:
  std::vector<Event> Submit(const NewOrder& order);
  Order Top(Side side) const;
  std::vector<LevelSnapshot> Depth(Side side, std::size_t levels) const;
  Quantity GetRestingQuantity(OrderId id) const;

 private:
  Event MakeEvent(EventType type, const NewOrder& order, RejectReason reason);

  std::map<PriceTicks, std::deque<Order>, std::greater<PriceTicks>> bids_;
  std::map<PriceTicks, std::deque<Order>> asks_;
  std::unordered_map<OrderId, std::pair<Side, PriceTicks>> live_;
  Sequence next_sequence_ = 1;
};

}  // namespace lob::ref

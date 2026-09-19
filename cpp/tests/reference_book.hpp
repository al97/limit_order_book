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
  std::vector<Event> Cancel(OrderId id);
  LevelSnapshot Top(Side side) const;
  std::vector<LevelSnapshot> Depth(Side side, std::size_t levels) const;
  Quantity GetRestingQuantity(OrderId id) const;

 private:
  Event Next(EventType type, OrderId id, OrderId counter, PriceTicks price,
             Quantity quantity, RejectReason reason);
  void Rest(const NewOrder& order, Quantity remaining);
  bool Compatible(Side taker_side, PriceTicks taker_price,
                  PriceTicks maker_price) const;
  template <class Levels>
  Quantity AvailableAgainst(const Levels& levels, const NewOrder& taker) const;
  template <class Levels>
  Quantity MatchAgainst(Levels& levels, const NewOrder& taker, Quantity remaining,
                        std::vector<Event>& events);

  std::map<PriceTicks, std::deque<Order>, std::greater<PriceTicks>> bids_;
  std::map<PriceTicks, std::deque<Order>> asks_;
  std::unordered_map<OrderId, std::pair<Side, PriceTicks>> live_;
  Sequence next_sequence_ = 1;
};

}  // namespace lob::ref

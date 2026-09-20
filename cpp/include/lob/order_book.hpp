#pragma once

#include "lob/types.hpp"
#include <vector>
#include <list>
#include <map>

namespace lob {
  class OrderBook {
    public:
      void AddOrder(const Order& order);
      Quantity GetVolumeAtPriceAndSide(PriceTicks price, Side side) const;

      std::vector<Event> Submit(const NewOrder& order);
      std::vector<Event> Cancel(OrderId id);
      LevelSnapshot Top(Side side);
      std::vector<LevelSnapshot> Depth(Side side, std::size_t levels) const;
      Quantity GetRestingQuantity(OrderId id) const;

    private:
      std::map<PriceTicks, std::list<Order>> sellSideMap;
      std::map<PriceTicks, std::list<Order>, std::greater<PriceTicks>> buySideMap;
      std::map<OrderId, std::list<Order>::iterator> locator;
      Event Emit(EventType type, OrderId order_id, OrderId counter_id = 0,
                 PriceTicks price = 0, Quantity quantity = 0,
                 RejectReason reason = RejectReason::None);
      Sequence sequence_{0};
      Sequence next_seq() { return ++sequence_; }
  };
}

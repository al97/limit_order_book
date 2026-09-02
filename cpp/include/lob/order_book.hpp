#pragma once

#include "lob/types.hpp"
#include <vector>

namespace lob {
  class OrderBook {
    public:
      void AddOrder(const Order& order);
      Quantity GetVolumeAtPriceAndSide(PriceTicks price, Side side) const;

      std::vector<Event> Submit(const NewOrder& order);
      std::vector<Event> Cancel(OrderId id);
      Order Top(Side side);
      std::vector<LevelSnapshot> Depth(Side side, std::size_t levels) const;
      Quantity GetRestingQuantity(OrderId id);

    private:
      std::vector<Order> orders;
  };
}

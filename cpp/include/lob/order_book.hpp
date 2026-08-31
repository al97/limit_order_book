#pragma once

#include "lob/types.hpp"
#include <vector>

namespace lob {
  class OrderBook {
    public:
      void AddOrder(const Order& order);
      Quantity GetVolumeAtPriceAndSide(PriceTicks price, Side side) const;

    private:
      std::vector<Order> orders;
  };
}

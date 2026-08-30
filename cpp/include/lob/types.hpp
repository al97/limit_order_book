#pragma once

#include <cstdint>

namespace lob {
  using OrderId = std::uint64_t;
  using PriceTicks = std::int64_t;
  using Quantity = std::uint64_t;
  enum class Side : std::uint8_t { Buy, Sell };

  struct Order {
    OrderId id;
    Side side;
    PriceTicks price;
    Quantity quantity;
  };
} // namespace lob
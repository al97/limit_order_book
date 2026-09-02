#pragma once

#include <cstdint>

namespace lob {
  using OrderId = std::uint64_t;
  using PriceTicks = std::int64_t;
  using Quantity = std::uint64_t;
  using Sequence = std::uint64_t;
  enum class Side : std::uint8_t { Buy, Sell };
  enum class TimeInForce : std::uint8_t { GTC, FOK, IOC, Market };

  struct Order {
    OrderId id;
    Side side;
    PriceTicks price;
    Quantity quantity;
  };

  struct LevelSnapshot {
    PriceTicks price;
    Quantity quantity;
  };

  struct NewOrder {
    OrderId id;
    Side side;
    PriceTicks price;
    Quantity quantity;
    TimeInForce tif;
  };

  enum class EventType : std::uint8_t {
    Filled,
    Cancelled, Rejected,
    Accepted, // live trade in book
    Trade // two orders matched and trade occurred
  };

  enum class RejectReason : std::uint8_t {
    None, DuplicateOrderId, ZeroQuantity, UnknownOrder
  };

  struct Event {
    Sequence sequence;
    EventType type;
    OrderId orderId;
    OrderId counterOrderId;
    PriceTicks price;
    Quantity quantity;
    RejectReason reason;
  };

} // namespace lob
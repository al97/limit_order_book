#pragma once

#include "lob/types.hpp"

#include <variant>
#include <vector>

namespace lob::test {

struct SubmitCommand {
  lob::NewOrder order;
};

struct CancelCommand {
  lob::OrderId id;
};

using Command = std::variant<SubmitCommand, CancelCommand>;
using Commands = std::vector<Command>;

inline Command SubmitOrder(lob::OrderId id, lob::Side side, lob::PriceTicks price,
                           lob::Quantity quantity, lob::TimeInForce tif) {
  return SubmitCommand{lob::NewOrder{id, side, price, quantity, tif}};
}

inline Command SubmitGtc(lob::OrderId id, lob::Side side, lob::PriceTicks price,
                         lob::Quantity quantity) {
  return SubmitOrder(id, side, price, quantity, lob::TimeInForce::GTC);
}

inline Command CancelOrder(lob::OrderId id) { return CancelCommand{id}; }

}  // namespace lob::test

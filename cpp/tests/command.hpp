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

inline Command SubmitGtc(lob::OrderId id, lob::Side side, lob::PriceTicks price,
                         lob::Quantity quantity) {
  return SubmitCommand{
      lob::NewOrder{id, side, price, quantity, lob::TimeInForce::GTC}};
}

inline Command CancelOrder(lob::OrderId id) { return CancelCommand{id}; }

}  // namespace lob::test

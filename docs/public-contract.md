# Public contract draft

Working draft for Milestone 0. One file so we can mark it up together before it becomes headers.

Albert owns the eventual `include/lob/` split. This document is the agreed shape, not production code. Nothing here compiles.

## Scalar types

```cpp
using OrderId  = std::uint64_t;
using Price    = std::int64_t;   // integer ticks, never a float
using Quantity = std::uint64_t;
using Sequence = std::uint64_t;  // engine-assigned, never caller-supplied
```

These are aliases, not classes. `Sequence sequence;` is a `uint64_t`. The names exist so signatures read clearly. They do not give type safety. Passing an `OrderId` where a `Quantity` belongs still compiles.

## Enums

```cpp
enum class Side { Buy, Sell };

enum class TimeInForce { GTC, IOC, FOK, Market };

enum class EventType { Trade, Rested, Cancelled, Rejected };

enum class RejectReason {
    None,
    DuplicateOrderId,
    ZeroQuantity,
    UnknownOrder,
    InsufficientLiquidity
};
```

`RejectReason::None` exists so unused fields have a defined value. See Event rules below.

`Market` lives in `TimeInForce` because Milestone 0 lists four enums and `OrderType` is not among them. That is an open question.

## Input and output objects

```cpp
struct NewOrder {
    OrderId     id;
    Side        side;
    Price       price;          // ignored when time_in_force == Market
    Quantity    quantity;
    TimeInForce time_in_force;
};

struct Event {
    Sequence     sequence;
    EventType    type;
    OrderId      order_id;         // the order this event is about
    OrderId      counterparty_id;  // Trade only, else 0
    Price        price;            // Trade fill price, else 0
    Quantity     quantity;         // Trade fill qty, else 0
    RejectReason reason;           // Rejected only, else None
};

struct TopOfBook {
    std::optional<Price>    bid_price;
    std::optional<Quantity> bid_quantity;
    std::optional<Price>    ask_price;
    std::optional<Quantity> ask_quantity;
};

struct Level {
    Price    price;
    Quantity quantity;
};

struct DepthSnapshot {
    std::vector<Level> bids;  // highest first
    std::vector<Level> asks;  // lowest first
};
```

The caller supplies `NewOrder::id`. Duplicate detection requires it.

The caller does not supply `Sequence`. The engine assigns it on arrival so a client cannot buy FIFO priority.

On a `Trade`, `order_id` is the taker and `counterparty_id` is the maker. That is a proposed convention. See open questions.

## OrderBook operations

```cpp
class OrderBook {
public:
    std::vector<Event>      submit(const NewOrder& order);
    std::vector<Event>      cancel(OrderId id);
    TopOfBook               top() const;
    DepthSnapshot           depth(std::size_t levels) const;
    std::optional<Quantity> resting_quantity(OrderId id) const;
};
```

**`submit`** places an order. This is the only operation that can create trades. The engine validates the order, matches it against the other side as far as the price allows, then rests or discards the leftover according to `time_in_force`. Returns every event the command produced. One taker can fill several makers and then rest a remainder, so the return is a vector.

**`cancel`** removes a resting order. It does not create trades. Unknown or already-filled IDs return one `Rejected` event with `UnknownOrder`. The return is a vector so the shape matches `submit`.

**`top`** returns the best bid and best ask and the quantity at each. It changes nothing. Empty sides are `std::nullopt`.

**`depth`** returns the best N price levels on each side with the total quantity at each. Bids are highest first. Asks are lowest first. It changes nothing.

**`resting_quantity`** returns how much of one order is still live. Empty means the ID is unknown or fully filled. Those are the same case, matching the cancel rule.

`submit` and `cancel` mutate the book. `top`, `depth`, and `resting_quantity` only read it.

## Event rules that the structs cannot express

Every field unused by an event type has a defined value. Unused IDs, prices, and quantities are `0`. Unused `reason` is `None`. If one implementation leaves a field uninitialized and the other writes zero, the differential test fails on a field neither of us cares about.

Event order for one command:

1. Trades first, in the order the makers were matched.
2. The taker's `Rested` or `Cancelled` event last.
3. A rejected command produces exactly one `Rejected` event and mutates nothing.

A GTC buy for 100 that fills two makers and rests 30 produces trade, trade, rested. In that order, every time.

## Locked behavior from the development plan

These are already agreed in `docs/development-plan.md`. Listed so we can check we read them the same way.

- Duplicate order ID rejects without mutation.
- Zero quantity rejects without mutation.
- Unknown or already-filled cancellation returns `UnknownOrder`.
- Trades occur at the resting maker's price.
- A Market order never rests.
- An insufficient FOK order creates no trades and no mutation.
- Engine sequence, not wall-clock time, determines FIFO order.

## Open questions for Albert

1. **Market as a time-in-force.** The plan lists four enums. Folding Market into `TimeInForce` matches the plan and keeps `NewOrder` smaller. Adding `OrderType { Limit, Market }` is the cleaner model. Brian leans toward keeping the four enums.

2. **Empty book from `top`.** Four optionals so one empty side is representable. A sentinel price would be faster. Milestone 1 compares both books on this, so it has to match.

3. **Taker or maker in `order_id` on a trade.** Draft says taker. Fine either way. Not fine if we each assume the other.

4. **Zero-fill unused event fields.** Needs to be binding, not a convention, or the differential checker turns into a field-by-field argument.

## Out of scope

REST, JSON, WebSocket, and the local UI. Those wrap `submit` and `cancel` after the headers stop moving.

`std::vector<Event>` allocates on every submit. Ignore that until Milestone 5. A caller-provided buffer can replace it without changing these semantics.

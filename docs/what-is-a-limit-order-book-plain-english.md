# What is a limit order book?

A plain-English technical briefing for a software engineer who is new to
financial markets.

This document explains:

- why an order book exists,
- what the financial vocabulary means,
- what the matching engine actually does,
- which data structures fit the problem,
- which correctness rules matter, and
- what we should build in this repository.

It deliberately does **not** attempt to explain every exchange rule or every
possible order type. The goal is to establish a mental model that is simple
enough to remember and precise enough to implement.

## The whole idea in one story

Forget stocks for a moment. Imagine a marketplace for used bicycles.

Alice says:

> I will sell my bicycle for $200 or more.

Bob says:

> I will buy a bicycle for $180 or less.

There is no deal. Alice will not accept $180 and Bob will not pay $200. Their
offers wait.

Carol then says:

> I will buy a bicycle for as much as $210.

Carol can trade with Alice. Alice was already waiting and offered to sell for
$200, so the trade happens at **$200**, not $210. Carol's $210 was a maximum,
not a request to overpay.

A limit order book is the software version of those waiting offers:

- buyers wait on one side,
- sellers wait on the other side,
- the most competitive prices are considered first, and
- people at the same price are served in arrival order.

The matching engine is the clerk who receives each new instruction and either:

1. pairs it with somebody already waiting,
2. puts its unfilled remainder into the correct waiting line, or
3. discards the remainder when the instruction says not to wait.

That is the core system.

## What we are building

We are building a **matching engine for one instrument**.

An instrument is one tradeable thing: one stock, future, option contract, or
other product. AAPL and MSFT have separate books.

The engine receives a totally ordered stream of commands:

- submit an order,
- cancel an order, and eventually
- replace an order.

It produces:

- trades,
- acceptances and cancellations,
- rejections, and
- the new state of the book.

The engine is a deterministic state machine:

> Same starting state + same commands in the same order = same trades and same
> final book.

That determinism is more important than clever code. A fast engine that
occasionally changes the result is unusable.

## The vocabulary

### Bid and ask

A **bid** is a waiting offer to buy. The **best bid** is the highest price any
waiting buyer will pay.

An **ask** (or offer) is a waiting offer to sell. The **best ask** is the lowest
price any waiting seller will accept.

Higher bids are better for sellers, so bids are considered highest first.
Lower asks are better for buyers, so asks are considered lowest first.

Together, the best bid and best ask are the **top of book** or BBO: best bid
and offer.

### Spread

The **spread** is:

```text
best ask - best bid
```

If the best bid is $100.00 and the best ask is $100.02, the spread is $0.02.
It is the gap between the most eager buyer and the most eager seller. Nothing
trades while that gap remains.

### Limit order

A limit order specifies the worst price the owner will accept:

- a buy limit means “pay this price **or less**,”
- a sell limit means “receive this price **or more**.”

If it cannot be filled immediately, its remainder may wait in the book.

### Marketable limit order

A limit order is **marketable** when its price is already compatible with the
other side:

- a buy at or above the best ask,
- a sell at or below the best bid.

It behaves aggressively at first, but any remainder can still rest at its
limit.

### Market order

A market order means:

> Trade immediately against the best available prices. Do not leave a remainder
> waiting in the book.

It has no price protection. A large market order can consume several price
levels and receive progressively worse prices. This is one form of
**slippage**.

### Maker and taker

The **maker** was already resting in the book and made liquidity available.
The **taker** is the incoming order that trades immediately and takes that
liquidity.

The trade occurs at the **maker's price**.

If a sell is resting at 100 and an incoming buyer is willing to pay as much as
101, the trade occurs at 100. The buyer's limit is a ceiling, not the trade
price.

### Price-time priority

Orders are prioritized in two steps:

1. **Price:** the better price trades first.
2. **Time:** at the same price, the earlier order trades first.

This is also called FIFO: first in, first out.

For our project, “time” means the engine's monotonically increasing sequence
number. It does not mean the caller's wall-clock timestamp.

### Tick and quantity

A **tick** is the smallest allowed price movement. If the tick is one cent:

```text
$100.25 = 10,025 ticks
```

Store that value as an integer. Do not use `double` for prices. Floating-point
rounding has no place in a value used as a map key and correctness boundary.

Quantity is also an integer number of shares or lots.

## A complete worked example

Suppose the sell side contains:

```text
Price 100: Order 10 sells 3 shares, then Order 11 sells 5 shares
Price 101: Order 12 sells 10 shares
```

Order 10 arrived before order 11, so it is first in line at price 100.

Now order 20 arrives:

```text
Buy 6 shares with a limit of 101
```

The engine does the following:

1. The best ask is 100, which is no greater than the buyer's 101 limit.
2. It trades 3 shares with order 10 at **100**.
3. Order 10 is now empty and leaves the book.
4. The buyer still needs 3 shares.
5. It trades 3 shares with order 11 at **100**.
6. Order 11 remains in the book with 2 shares.
7. The buyer is complete, so matching stops.
8. Order 12 at 101 is untouched.

The resulting sell side is:

```text
Price 100: Order 11 sells 2 shares
Price 101: Order 12 sells 10 shares
```

This example demonstrates almost the entire engine: best price before worse
prices, FIFO at one price, maker-price execution, complete and partial fills,
quantity updates, and removal of an empty order.

## The book has two layers

A book is not best modeled as one giant collection of orders.

It has two layers:

1. **Price levels**, sorted from best to worst.
2. **Orders at each price**, stored in FIFO order.

```text
Bids: highest price first          Asks: lowest price first

100.00 -> [order 7] -> [order 9]   100.02 -> [order 4] -> [order 8]
 99.99 -> [order 3]                100.03 -> [order 2]
 99.98 -> [order 1]                100.05 -> [order 6]
```

Each price level also stores its aggregate remaining quantity. That lets us
answer “how many shares are available at 100.02?” without scanning every order
in that line.

Beside the two sides is a third structure:

```text
order ID -> exact location of the live order
```

That index exists because cancellations arrive by order ID. Without it, every
cancel would require searching the book.

## The straightforward C++ design

For a correctness-first implementation:

```cpp
using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::uint64_t;
using Sequence = std::uint64_t;

enum class Side : std::uint8_t {
  Buy,
  Sell,
};

struct Order {
  OrderId id;
  Side side;
  Price price;
  Quantity remaining_quantity;
  Sequence sequence;
};

struct PriceLevel {
  Quantity total_quantity;
  std::list<Order> orders;
};
```

The sides are ordered maps:

```cpp
std::map<Price, PriceLevel, std::greater<Price>> bids;
std::map<Price, PriceLevel, std::less<Price>> asks;
```

Therefore:

- `bids.begin()` is the best bid,
- `asks.begin()` is the best ask.

The cancellation index stores enough information to unlink the order directly:

```cpp
struct OrderLocator {
  Side side;
  Price price;
  std::list<Order>::iterator order;
};

std::unordered_map<OrderId, OrderLocator> order_index;
```

`std::list` is not especially cache-friendly, but it gives us the behavior we
need with little custom code:

- append to the end of a price queue,
- remove its first order,
- remove an arbitrary known order, and
- keep iterators to other orders valid.

That makes it a good first implementation and a poor final optimization. We
will measure before replacing it.

## Why not use one heap of orders?

A min-heap for asks and max-heap for bids explain best-price access nicely.
They are not inherently wrong.

The trouble appears when a cancellation removes the final order at a price.
The now-empty price level may be in the middle of the heap. Removing it
efficiently requires:

- a custom indexed heap,
- another map from price to heap position, or
- lazy deletion and cleanup when the dead level reaches the top.

All three approaches introduce more state that must remain synchronized.

An ordered map directly supports:

- best-price access,
- ordered walking across several price levels,
- insertion of a new price,
- deletion of any price, and
- depth snapshots.

So the first C++ version uses ordered maps containing FIFO queues. A heap can
be a later measured comparison, not an assumption that `O(log n)` tells the
whole story.

## Submitting an order

For an incoming buy with limit price `P` and remaining quantity `Q`:

1. Validate the command before mutating anything.
2. Look at the cheapest ask.
3. While `Q > 0` and the best ask price is at most `P`:
   1. Take the oldest order at that price.
   2. Fill `min(Q, maker_remaining)`.
   3. Emit a trade at the maker's price.
   4. Subtract the fill from the incoming order, maker, and level total.
   5. Remove the maker and its ID-index entry if it reaches zero.
   6. Remove the price level if it becomes empty.
4. If the incoming order still has quantity and may rest, append it to the bid
   level at `P`.

An incoming sell is symmetric:

- inspect the highest bid,
- continue while that bid is at least the sell limit.

“Symmetric” does not mean copy and paste without thought. Tests must exercise
both directions.

## Cancelling an order

Cancellation is where the ID index earns its keep.

Given an order ID:

1. Look it up in `order_index`.
2. If it is absent, return an unknown-order rejection.
3. Use the locator to find its side, price level, and list node.
4. Subtract its remaining quantity from the level total.
5. Unlink it from the FIFO.
6. Remove its ID-index entry.
7. Remove the price level if no orders remain.
8. Emit a cancellation event containing the quantity removed.

We use **active cancellation**: remove the order now.

The alternative is lazy cancellation: mark the order dead and skip it during a
future match. Lazy deletion can make the cancel command cheap, but it retains
dead nodes, forces the matching path to perform cleanup, complicates aggregate
quantities, and makes memory behavior harder to reason about.

Active cancellation is the better default for this learning engine.

## Time in force

Time in force controls what happens to an unfilled remainder.

### GTC: good till cancelled

Trade whatever is immediately available. Put the remainder in the book.

### IOC: immediate or cancel

Trade whatever is immediately available. Cancel the remainder instead of
resting it.

### FOK: fill or kill

Fill the entire quantity immediately or do nothing.

FOK requires a read-only pre-check. Walk acceptable price levels and determine
whether enough quantity exists **before** emitting a trade or changing state.
Rolling back completed trades is not an acceptable implementation.

### Market

Continue matching while the opposite side has orders, regardless of price.
Cancel any remainder when the opposite side becomes empty. A market order never
rests.

## The events are part of the design

The engine should not print trades or log from inside the matching loop.

It should return typed events such as:

- `Accepted`
- `Trade`
- `Filled`
- `Cancelled`
- `Rejected`

A trade event should contain the maker order ID, taker order ID, trade price,
trade quantity, and aggressor side.

Structured events make the engine easy to test, replay, and embed inside a
larger system. Strings and console output do not.

## Correctness invariants

After every public command:

1. Every resting order appears in exactly one price-level FIFO.
2. Every resting order has exactly one ID-index entry.
3. Filled and cancelled orders have no ID-index entry.
4. A level's aggregate quantity equals the sum of its live orders.
5. No empty price level remains in either map.
6. Orders at one price remain in arrival order.
7. Zero-quantity orders never rest.
8. If both sides are non-empty, `best_bid < best_ask`.

The final rule follows from continuous matching. If the best bid is equal to or
greater than the best ask, the engine has left compatible orders waiting
instead of trading them.

These invariants should exist as assertions in tests and, where practical, in
a debug-only internal validator.

## Complexity we can honestly claim

Let:

- `P` be the number of occupied price levels,
- `F` be the number of resting orders filled by one incoming order, and
- `L` be the number of price levels completely consumed.

Then the correctness-first design has these approximate costs:

| Operation | Cost | Why |
| --- | --- | --- |
| Read best bid or ask | `O(1)` | Read `map.begin()` |
| Rest an order | `O(log P)` | Find or create its price level |
| Append within a level | `O(1)` | Add to FIFO tail |
| Cancel a known order | Expected `O(1)` | Hash lookup and list unlink |
| Remove an empty level | `O(log P)` | Erase its price from the map |
| Match | `O(F + L log P)` | Visit fills and remove exhausted levels |
| Return the best `N` levels | `O(N)` | Totals are cached per level |

Big-O is not the final performance story. `std::map` and `std::list` allocate
nodes and chase pointers, which can miss CPU caches. They are chosen because
they make the rules obvious and reduce implementation risk.

Once the engine is correct and benchmarked, possible replacements include:

- pooled intrusive order nodes,
- a flat array indexed by price tick,
- a bitset of occupied levels, and
- cached best-level pointers.

Those are optimizations, not prerequisites for understanding the book.

## One book, one writer

Matching for one instrument needs one unambiguous order of events.

Putting several threads inside one book creates difficult questions:

- Which of two simultaneous orders arrived first?
- Can a cancellation race with a fill?
- Can readers observe half of a quantity update?

The simplest answer is one writer per book or shard. The surrounding system can
still be highly concurrent:

```text
network readers -> validation/gateway -> sequencer -> matching shard
                                                   -> trade reports
                                                   -> market-data events
                                                   -> journal
```

Different symbols can be assigned to different matching threads. Persistence,
networking, and downstream publishing can happen outside the matching path.

This is why a highly concurrent trading system can contain a deliberately
single-threaded matching engine.

## Matching engine versus order-book replica

Two different programs are often called an “order book.”

### Matching engine

The authoritative system we are building:

- accepts new orders and cancellations,
- decides which orders trade,
- emits fills, and
- owns the true live state.

### Book replica

A read-only reconstruction used by traders and market-data consumers:

- receives exchange events such as add, execute, cancel, and delete,
- applies them in sequence,
- reproduces the visible state, and
- cannot accept or match new orders.

The internal data structures can look similar, but the responsibilities are
different. Our first project is a matching engine, not an exchange-feed parser.

## What is deliberately outside version one

Real exchanges contain much more than the basic continuous FIFO matcher:

- opening and closing auctions,
- hidden and iceberg orders,
- self-trade prevention,
- trading halts,
- post-only orders,
- stop triggers,
- alternative allocation rules such as pro-rata,
- account and risk checks,
- fees,
- exchange protocols,
- persistence and recovery, and
- market-data distribution.

Knowing these exist is useful. Implementing them now would hide the central
lesson under venue-specific policy.

Version one should make the simple case boringly correct.

## What to build in this repository

### Milestone 1: types and skeleton

- Integer `Price`, `Quantity`, `OrderId`, and `Sequence`.
- `Side`, time-in-force, command, event, and rejection enums.
- CMake library and test targets.

### Milestone 2: resting book and queries

- Ordered bid and ask maps.
- FIFO orders within each price.
- Cached level quantity.
- Top-of-book, depth, and order lookup.

### Milestone 3: matching

- Buy and sell crossing.
- Partial and complete fills.
- Maker-price execution.
- FIFO at one price.
- Sweeps through several price levels.

### Milestone 4: active cancellation

- ID-to-iterator index.
- Cancellation from the head, middle, and tail of a FIFO.
- Removal of empty price levels.

### Milestone 5: order behavior

- GTC
- IOC
- Market
- FOK with a non-mutating pre-check

### Milestone 6: hardening

- Scenario tests.
- Debug invariant checks.
- A slow reference implementation for randomized differential tests.
- Address and undefined-behavior sanitizers.
- Only then, benchmarks.

## The tests that prove we understand it

At minimum:

1. A non-crossing order rests.
2. A buy at 101 trades with a sell at 100 at **100**.
3. Two orders at one price fill FIFO.
4. A partially filled maker keeps its place and correct remainder.
5. A GTC taker remainder rests.
6. An IOC remainder does not rest.
7. A market order walks several levels.
8. An insufficient FOK order makes no changes.
9. Cancelling the final order at a price removes the level.
10. Cancelling the head, middle, or tail does not reorder survivors.
11. Duplicate IDs, zero quantity, and unknown cancellation reject without
    mutation.
12. Every invariant remains true after a randomized command stream.

Tests should assert the exact trade sequence and final book, not only the final
best bid and ask.

## Common mistakes

### Treating the incoming limit as the trade price

It is a ceiling for a buyer or floor for a seller. The resting order sets the
trade price.

### Using floating-point prices

Price comparison is an invariant. Use integer ticks.

### Keeping one global heap of orders

It optimizes “show me the next order” while fighting price-level iteration,
depth, and cancellation. Model levels and FIFO queues explicitly.

### Using a vector and `swap_remove` for FIFO orders

It removes quickly by moving a later order into the gap, which destroys arrival
priority.

### Forgetting aggregate quantity updates

Every fill and cancellation must update the order, price-level total, and ID
index consistently.

### Leaving empty price levels behind

Then `begin()` can report a “best” price with no actual liquidity.

### Making the book internally concurrent

It complicates event ordering before profiling has shown any need. Sequence
first; match on one writer.

### Optimizing before preserving a reference model

Keep a simple correct implementation or model. Faster data structures can then
be checked against it with identical command streams.

## How this relates to HFT work

This project is not a trading strategy. It will not predict prices or teach us
which stocks to buy.

It teaches the mechanism around which many trading systems are built:

- how orders wait,
- how liquidity is consumed,
- why queue position matters,
- why cancellation must be fast,
- why exact sequencing matters, and
- why predictable latency and correctness matter together.

The first implementation demonstrates domain fluency and sound software
design. The later optimization work demonstrates low-level systems thinking.
Neither should be confused with building an entire exchange.

## The shortest version to remember

If everything else fades, remember this:

1. Buyers are sorted highest price first.
2. Sellers are sorted lowest price first.
3. At one price, oldest order goes first.
4. Incoming orders trade against the other side while prices are compatible.
5. Trades happen at the resting order's price.
6. Compatible quantity is removed; permitted leftovers rest.
7. A hash index makes cancellation direct.
8. Integer ticks, cached level totals, and deterministic sequencing protect
   correctness.

That is a limit order book.

## Starting reference

- [Design A Limit Order Book — Google SWE Teaches Low Level Design](https://www.youtube.com/watch?v=nmYx6tQxtSs)

The video provides the core mental model: best-price structures, FIFO queues by
price, cached volume, an order-ID index, and an explicit cancellation policy.
This document keeps that model while choosing correctness-first C++ containers
and spelling out the invariants the implementation must preserve.

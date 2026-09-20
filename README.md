# Limit order book

This repository is a C++20 limit order book matching engine. It is a learning project, not a production exchange.

Read [About limit order books](docs/what-is-a-limit-order-book.md) for the domain. The [Limit Order Book Development Plan](docs/development-plan.md) is the checked-in tracker.

The matcher accepts client `Submit` and `Cancel` commands and emits trades. A separate, slower reference book in `cpp/tests/` receives the same commands so tests can compare results. That reference is not a second product. A later market-data replica, if added, would reconstruct an external venue's book and must not feed orders into this matcher.

## Build and test

You need CMake 3.28 or newer and a C++20 compiler.

From the repository root, run the same sequence as the `warnings` job in [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

```bash
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" && cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

`ctest` runs `types_test`, `order_book_test`, `reference_book_test`, `agree_test`, and `differential_test`.

If you want sanitizers, follow the `asan` and `ubsan` jobs in `.github/workflows/ci.yml`.

## Example

```cpp
#include "lob/order_book.hpp"

lob::OrderBook book;
book.Submit({1, lob::Side::Sell, 100, 3, lob::TimeInForce::GTC});
const auto fills = book.Submit({2, lob::Side::Buy, 101, 10, lob::TimeInForce::GTC});
book.Cancel(2);
```

The buy trades 3 shares at the resting sell price of 100. The leftover 7 rests on the bid side, then `Cancel(2)` removes it.

## Time-in-force

- **GTC** matches what it can and rests the remainder.
- **IOC** matches what it can and cancels the remainder. Nothing rests.
- **Market** matches while the opposite side has size, ignoring the submitted price, and never rests.
- **FOK** fills the entire quantity now or rejects with no trades and no book change.

## Unsupported in this version

Replace/modify, post-only, hidden and iceberg orders, stop orders, pro-rata allocation, self-trade prevention, auctions, trading halts, fees, multi-instrument matching, persistence, and network protocols are out of scope.

## Reproducing a differential failure

`differential_test` uses fixed seeds. On a mismatch it prints `seed=N` and writes `failing_seed.txt` with a minimized command list. Re-run the same `ctest` binary to reproduce. The minimizer keeps the shortest failing prefix, then drops commands that are not required to reproduce that failure kind.

# Limit order book

This repository is a C++20 limit order book matching engine. It is a learning project, not a production exchange.

Read [About limit order books](docs/what-is-a-limit-order-book.md) for the domain. The [Limit Order Book Development Plan](docs/development-plan.md) is the checked-in tracker.

## Build and test

You need CMake 3.28 or newer and a C++20 compiler.

From the repository root, run the same sequence as the `warnings` job in [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

```bash
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" && cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

`ctest` runs `types_test` and `order_book_test`.

If you want sanitizers, follow the `asan` and `ubsan` jobs in `.github/workflows/ci.yml`.

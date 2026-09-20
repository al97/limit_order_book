# Matcher benchmarks

These numbers are a laptop baseline for the correctness-first book: `std::map` price levels, `std::list` FIFO queues, `std::map` order-id locator. They are not an exchange SLA. Do not quote them as production matching latency.

The replica in optional Milestone 4 is a different program. It reconstructs an external venue's book from market data. It does not call `OrderBook::Submit`. These benches time the matcher only.

## How to reproduce

From the repository root, Release only. Debug and sanitizer builds are the wrong surface.

```bash
cmake -S cpp -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" && cmake --build build-release --parallel --target lob_bench && ./build-release/lob_bench
```

Default is 10 independent repetitions, each on a fresh book. Pass a count to change it: `./build-release/lob_bench 20`. `lob_bench` is not part of `ctest`. Replace the captured numbers below when the containers or matching loop change.

## Workloads

| name | what is timed |
| --- | --- |
| rest a bid | `Submit` of a GTC bid that does not cross. One price level. FIFO grows. |
| cancel | `Cancel` of a shuffled live id from a 10,000-order bid level. |
| take 1 / 10 / 100 makers | One GTC buy that fully fills that many resting sells. Setup submits are not timed. |
| mixed replay | Each command in a 100,000-step GTC submit/cancel stream, same public API as the tests. |

Each sample is one public call. Each run takes p50 and p99 over those samples. The printed **typical** is the median of the ten run-level p50s; **tail** is the median of the run-level p99s. Values under 1 µs stay in nanoseconds.

## Captured run

```
compiler: Clang 21.0.0 (clang-2100.1.1.101)
build: Release
cpu: Apple M1 Pro
repeats: 10 (median across runs)
containers: std::map price levels, std::list FIFO, std::map id locator
clock: std::chrono::steady_clock
typical = p50,  tail = p99

workload             typical      tail
rest a bid            125 ns    167 ns
cancel                166 ns    292 ns
take 1 maker          125 ns    167 ns
take 10 makers        583 ns    708 ns
take 100 makers      4.6 µs    5.8 µs
mixed replay          167 ns    334 ns
```

Host OS: macOS. Single thread. No CPU pinning, no cache isolation, no persistence, no network.

Take latency grows with the number of makers walked: 1 maker is as cheap as resting a bid; 100 makers is about 4.6 µs typical on this CPU. Keep that shape when someone later replaces the maps.

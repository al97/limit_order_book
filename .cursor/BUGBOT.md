# Limit Order Book Review Instructions

Read these documents before reviewing implementation changes:

- `docs/development-plan.md`
- `docs/what-is-a-limit-order-book-plain-english.md`

Review for correctness before performance. Flag violations of these rules:

- Prices and quantities use integer ticks/lots, never floating point.
- Matching uses price-time priority: best price first, then FIFO at one price.
- Every trade executes at the resting maker's price.
- Partial fills preserve a resting order's FIFO position.
- Market orders never rest.
- IOC cancels its unfilled remainder.
- Insufficient FOK rejects without trades or any book mutation.
- Duplicate IDs, zero quantity, and unknown cancels reject without mutation.
- Active cancellation updates the order node, price-level total, and ID index.
- Filled and cancelled orders leave no stale ID-index entries.
- Empty price levels are removed immediately.
- Level totals equal the sum of their live order quantities.
- A continuous book never remains locked or crossed after a command.
- Quantity arithmetic cannot underflow or overflow.
- Iterator, pointer, and reference lifetimes remain valid after container edits.
- One instrument has one deterministic writer; do not add locking internally
  without an explicit architectural change.
- Provider/API types and network operations stay outside the matching engine.
- Tests assert exact event order and observable snapshots, not private
  containers.
- Production matching logic is not reused by the independent reference model.

Require tests for every behavior change. Treat latency claims as unproven unless
supported by reproducible benchmarks that identify build mode, workload, and
hardware. Do not recommend low-level optimization before correctness,
differential tests, and sanitizers pass.

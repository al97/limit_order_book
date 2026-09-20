Non-crossing GTC rests:

Start: empty book
Command: Submit Buy (id = 1, price = 100, qty = 10, GTC)

Expected events:
  Accepted(id = 1)

Expected final state:
  Bid 100: 10 shares (order 1)
  No trades


Cross at Maker price:

Start: Sell id=1 resting at 100, qty = 3
Command: Submit Buy (id = 2, price = 101, qty=3, GTC)

Expected events:
  Accepted(id=2)
  Trade(maker=1, taker=2, price=100, qty=3)
  Filled(id=1)
  Filled(id=2)

Expected final state:
  Empty book(both fully filled)


FIFO at one price:

Start:
  Sell id=1 at 100, qty=3
  Sell id=2 at 100, qty=5
Command: Submit Buy id=3, price=100, qty=4, GTC

Expected events:
  Accepted(id=3)
  Trade(maker=1, taker=3, price=100, qty=3) // id=1 first (FIFO)
  Filled(id=1)
  Trade(maker=2, taker=3, price=100, qty=1) // then partial from id=2
  Filled(id=3)

Expected final state:
  Ask 100: 4 shares (order 2 remainder)


IOC cancels remainder:

Start: Sell id=1 resting at 100, qty = 3
Command: Submit Buy id=2, price=101, qty=10, IOC

Expected events:
  Accepted(id=2)
  Trade(maker=1, taker=2, price=100, qty=3)
  Filled(id=1)
  Cancelled(id=2, qty=7)

Expected final state:
  Empty book (remainder did not rest)


Market ignores price and never rests:

Start:
  Sell id=1 at 100, qty=3
  Sell id=2 at 110, qty=5
Command: Submit Buy id=3, price=1, qty=10, Market

Expected events:
  Accepted(id=3)
  Trade(maker=1, taker=3, price=100, qty=3)
  Trade(maker=2, taker=3, price=110, qty=5)
  Filled(id=1)
  Filled(id=2)
  Cancelled(id=3, qty=2)

Expected final state:
  Empty book (market remainder did not rest)


Insufficient FOK rejects without mutation:

Start:
  Sell id=1 at 100, qty=3
  Sell id=2 at 110, qty=5
Command: Submit Buy id=3, price=101, qty=8, FOK

Expected events:
  Rejected(id=3)

Expected final state:
  Ask 100: 3 shares (order 1)
  Ask 110: 5 shares (order 2)
  No trades
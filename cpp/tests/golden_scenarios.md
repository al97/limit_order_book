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
  Trade(maker=2, taker=3, price=100, qty=1) // then partial from id=2
  Filled(id=1)
  Filled(id=3)

Expected final state:
  Ask 100: 4 shares (order 2 remainder)
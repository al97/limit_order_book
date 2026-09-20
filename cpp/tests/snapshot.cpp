#include "snapshot.hpp"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>
#include <variant>

namespace {

constexpr std::size_t kFullDepth = 32;

bool EqualLevel(const lob::LevelSnapshot& left, const lob::LevelSnapshot& right) {
  return left.price == right.price && left.quantity == right.quantity;
}

bool EqualLevels(const std::vector<lob::LevelSnapshot>& left,
                 const std::vector<lob::LevelSnapshot>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (!EqualLevel(left[i], right[i])) {
      return false;
    }
  }
  return true;
}

const char* SideName(lob::Side side) {
  return side == lob::Side::Buy ? "Buy" : "Sell";
}

const char* EventName(lob::EventType type) {
  switch (type) {
    case lob::EventType::Filled:
      return "Filled";
    case lob::EventType::Cancelled:
      return "Cancelled";
    case lob::EventType::Rejected:
      return "Rejected";
    case lob::EventType::Accepted:
      return "Accepted";
    case lob::EventType::Trade:
      return "Trade";
  }
  return "?";
}

const char* TifName(lob::TimeInForce tif) {
  switch (tif) {
    case lob::TimeInForce::GTC:
      return "GTC";
    case lob::TimeInForce::FOK:
      return "FOK";
    case lob::TimeInForce::IOC:
      return "IOC";
    case lob::TimeInForce::Market:
      return "Market";
  }
  return "?";
}

const char* ReasonName(lob::RejectReason reason) {
  switch (reason) {
    case lob::RejectReason::None:
      return "None";
    case lob::RejectReason::DuplicateOrderId:
      return "DuplicateOrderId";
    case lob::RejectReason::ZeroQuantity:
      return "ZeroQuantity";
    case lob::RejectReason::UnknownOrder:
      return "UnknownOrder";
  }
  return "?";
}

void AppendLevels(std::ostringstream& out, const char* name,
                  const std::vector<lob::LevelSnapshot>& levels) {
  out << "  " << name << ":\n";
  if (levels.empty()) {
    out << "    (none)\n";
    return;
  }
  for (const lob::LevelSnapshot& level : levels) {
    out << "    price=" << level.price << " qty=" << level.quantity << "\n";
  }
}

template <class Book>
std::vector<lob::Event> Apply(Book& book, const lob::test::Command& command) {
  return std::visit(
      [&](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, lob::test::SubmitCommand>) {
          return book.Submit(payload.order);
        } else {
          return book.Cancel(payload.id);
        }
      },
      command);
}

void TrackId(std::vector<lob::OrderId>& ids, const lob::test::Command& command) {
  std::visit(
      [&](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, lob::test::SubmitCommand>) {
          ids.push_back(payload.order.id);
        } else {
          ids.push_back(payload.id);
        }
      },
      command);
}

std::optional<lob::OrderId> SubmitId(const lob::test::Command& command) {
  if (const auto* submit = std::get_if<lob::test::SubmitCommand>(&command)) {
    return submit->order.id;
  }
  return std::nullopt;
}

const lob::Event* FirstOf(const std::vector<lob::Event>& events, lob::EventType type) {
  for (const lob::Event& event : events) {
    if (event.type == type) {
      return &event;
    }
  }
  return nullptr;
}

std::string FormatCommand(const lob::test::Command& command) {
  std::ostringstream out;
  std::visit(
      [&](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, lob::test::SubmitCommand>) {
          const lob::NewOrder& order = payload.order;
          out << "Submit " << SideName(order.side) << " id=" << order.id
              << " price=" << order.price << " qty=" << order.quantity
              << " tif=" << TifName(order.tif);
        } else {
          out << "Cancel id=" << payload.id;
        }
      },
      command);
  return out.str();
}

}  // namespace

bool operator==(const ObservableSnapshot& left, const ObservableSnapshot& right) {
  return EqualLevel(left.bid_top, right.bid_top) &&
         EqualLevel(left.ask_top, right.ask_top) &&
         EqualLevels(left.bids, right.bids) && EqualLevels(left.asks, right.asks) &&
         left.live == right.live;
}

bool operator==(const TradeFact& left, const TradeFact& right) {
  return left.maker == right.maker && left.taker == right.taker &&
         left.price == right.price && left.quantity == right.quantity;
}

ObservableSnapshot Capture(lob::OrderBook& book, const std::vector<lob::OrderId>& ids,
                           std::size_t depth) {
  ObservableSnapshot snapshot;
  snapshot.bid_top = book.Top(lob::Side::Buy);
  snapshot.ask_top = book.Top(lob::Side::Sell);
  snapshot.bids = book.Depth(lob::Side::Buy, depth);
  snapshot.asks = book.Depth(lob::Side::Sell, depth);
  for (const lob::OrderId id : ids) {
    snapshot.live.push_back({id, book.GetRestingQuantity(id)});
  }
  return snapshot;
}

ObservableSnapshot Capture(const lob::ref::ReferenceBook& book,
                           const std::vector<lob::OrderId>& ids, std::size_t depth) {
  ObservableSnapshot snapshot;
  snapshot.bid_top = book.Top(lob::Side::Buy);
  snapshot.ask_top = book.Top(lob::Side::Sell);
  snapshot.bids = book.Depth(lob::Side::Buy, depth);
  snapshot.asks = book.Depth(lob::Side::Sell, depth);
  for (const lob::OrderId id : ids) {
    snapshot.live.push_back({id, book.GetRestingQuantity(id)});
  }
  return snapshot;
}

std::string FormatHistory(const std::vector<lob::NewOrder>& history) {
  std::ostringstream out;
  out << "command history:\n";
  if (history.empty()) {
    out << "  (empty)\n";
    return out.str();
  }
  for (std::size_t i = 0; i < history.size(); ++i) {
    const lob::NewOrder& order = history[i];
    out << "  " << i << ": Submit " << SideName(order.side) << " id=" << order.id
        << " price=" << order.price << " qty=" << order.quantity << "\n";
  }
  return out.str();
}

std::string FormatHistory(const lob::test::Commands& history) {
  std::ostringstream out;
  out << "command history:\n";
  if (history.empty()) {
    out << "  (empty)\n";
    return out.str();
  }
  for (std::size_t i = 0; i < history.size(); ++i) {
    out << "  " << i << ": " << FormatCommand(history[i]) << "\n";
  }
  return out.str();
}

std::string FormatSnapshot(const ObservableSnapshot& snapshot) {
  std::ostringstream out;
  out << "  bid_top price=" << snapshot.bid_top.price
      << " qty=" << snapshot.bid_top.quantity << "\n";
  out << "  ask_top price=" << snapshot.ask_top.price
      << " qty=" << snapshot.ask_top.quantity << "\n";
  AppendLevels(out, "bids", snapshot.bids);
  AppendLevels(out, "asks", snapshot.asks);
  out << "  live:\n";
  if (snapshot.live.empty()) {
    out << "    (none)\n";
  }
  for (const auto& [id, quantity] : snapshot.live) {
    out << "    id=" << id << " qty=" << quantity << "\n";
  }
  return out.str();
}

std::string FormatEvents(const std::vector<lob::Event>& events) {
  std::ostringstream out;
  if (events.empty()) {
    out << "  (none)\n";
    return out.str();
  }
  for (const lob::Event& event : events) {
    out << "  " << EventName(event.type) << " id=" << event.orderId
        << " counter=" << event.counterOrderId << " price=" << event.price
        << " qty=" << event.quantity << " reason=" << ReasonName(event.reason)
        << "\n";
  }
  return out.str();
}

std::string FormatTrades(const std::vector<TradeFact>& trades) {
  std::ostringstream out;
  if (trades.empty()) {
    out << "  (none)\n";
    return out.str();
  }
  for (const TradeFact& trade : trades) {
    out << "  maker=" << trade.maker << " taker=" << trade.taker
        << " price=" << trade.price << " qty=" << trade.quantity << "\n";
  }
  return out.str();
}

bool BookIsUncrossed(const ObservableSnapshot& snapshot) {
  if (snapshot.bid_top.quantity == 0 || snapshot.ask_top.quantity == 0) {
    return true;
  }
  return snapshot.bid_top.price < snapshot.ask_top.price;
}

const char* InvariantFailure(const ObservableSnapshot& snapshot) {
  if (!BookIsUncrossed(snapshot)) {
    return "crossed or locked book";
  }
  if (snapshot.bids.empty()) {
    if (snapshot.bid_top.quantity != 0) {
      return "bid top present on empty bid depth";
    }
  } else if (!EqualLevel(snapshot.bid_top, snapshot.bids.front())) {
    return "bid top does not match first depth level";
  }
  if (snapshot.asks.empty()) {
    if (snapshot.ask_top.quantity != 0) {
      return "ask top present on empty ask depth";
    }
  } else if (!EqualLevel(snapshot.ask_top, snapshot.asks.front())) {
    return "ask top does not match first depth level";
  }
  for (const lob::LevelSnapshot& level : snapshot.bids) {
    if (level.quantity == 0) {
      return "empty bid level in depth";
    }
  }
  for (const lob::LevelSnapshot& level : snapshot.asks) {
    if (level.quantity == 0) {
      return "empty ask level in depth";
    }
  }
  return nullptr;
}

std::vector<TradeFact> TradesFrom(const std::vector<lob::Event>& events,
                                  lob::OrderId taker) {
  std::vector<TradeFact> trades;
  for (const lob::Event& event : events) {
    if (event.type != lob::EventType::Trade) {
      continue;
    }
    // Production stores the taker in orderId. Reference stores the maker.
    const lob::OrderId maker =
        event.orderId == taker ? event.counterOrderId : event.orderId;
    trades.push_back(TradeFact{maker, taker, event.price, event.quantity});
  }
  return trades;
}

StreamDiff FirstDiff(const lob::test::Commands& commands) {
  lob::OrderBook production;
  lob::ref::ReferenceBook reference;
  std::vector<lob::OrderId> ids;
  StreamDiff diff;

  const ObservableSnapshot empty_prod = Capture(production, ids, kFullDepth);
  const ObservableSnapshot empty_ref = Capture(reference, ids, kFullDepth);
  if (empty_prod != empty_ref) {
    diff.kind = DiffKind::Snapshot;
    diff.detail = "empty books disagree";
    return diff;
  }
  if (const char* problem = InvariantFailure(empty_prod); problem != nullptr) {
    diff.kind = DiffKind::Invariant;
    diff.detail = problem;
    return diff;
  }

  for (std::size_t i = 0; i < commands.size(); ++i) {
    diff.command_index = i;
    const lob::test::Command& command = commands[i];
    TrackId(ids, command);
    const ObservableSnapshot before_prod = Capture(production, ids, kFullDepth);
    const ObservableSnapshot before_ref = Capture(reference, ids, kFullDepth);
    const std::vector<lob::Event> production_events = Apply(production, command);
    const std::vector<lob::Event> reference_events = Apply(reference, command);
    const ObservableSnapshot production_snap = Capture(production, ids, kFullDepth);
    const ObservableSnapshot reference_snap = Capture(reference, ids, kFullDepth);

    const lob::Event* production_reject =
        FirstOf(production_events, lob::EventType::Rejected);
    const lob::Event* reference_reject =
        FirstOf(reference_events, lob::EventType::Rejected);
    if ((production_reject == nullptr) != (reference_reject == nullptr) ||
        (production_reject != nullptr &&
         production_reject->reason != reference_reject->reason)) {
      diff.kind = DiffKind::Reject;
      std::ostringstream out;
      out << "rejections disagree at command " << i << "\n";
      out << "production events:\n" << FormatEvents(production_events);
      out << "reference events:\n" << FormatEvents(reference_events);
      diff.detail = out.str();
      return diff;
    }

    if (production_reject != nullptr) {
      if (production_snap != before_prod || reference_snap != before_ref) {
        diff.kind = DiffKind::Mutation;
        std::ostringstream out;
        out << "rejected command mutated the book at command " << i << "\n";
        out << "production before:\n" << FormatSnapshot(before_prod);
        out << "production after:\n" << FormatSnapshot(production_snap);
        out << "reference before:\n" << FormatSnapshot(before_ref);
        out << "reference after:\n" << FormatSnapshot(reference_snap);
        diff.detail = out.str();
        return diff;
      }
    }

    if (const auto taker = SubmitId(command); taker.has_value()) {
      const std::vector<TradeFact> production_trades =
          TradesFrom(production_events, *taker);
      const std::vector<TradeFact> reference_trades =
          TradesFrom(reference_events, *taker);
      if (production_trades != reference_trades) {
        diff.kind = DiffKind::Trade;
        std::ostringstream out;
        out << "trades disagree at command " << i << "\n";
        out << "production trades:\n" << FormatTrades(production_trades);
        out << "reference trades:\n" << FormatTrades(reference_trades);
        out << "production events:\n" << FormatEvents(production_events);
        out << "reference events:\n" << FormatEvents(reference_events);
        diff.detail = out.str();
        return diff;
      }
    }

    const lob::Event* production_cancel =
        FirstOf(production_events, lob::EventType::Cancelled);
    const lob::Event* reference_cancel =
        FirstOf(reference_events, lob::EventType::Cancelled);
    if ((production_cancel == nullptr) != (reference_cancel == nullptr) ||
        (production_cancel != nullptr &&
         (production_cancel->orderId != reference_cancel->orderId ||
          production_cancel->quantity != reference_cancel->quantity ||
          production_cancel->price != reference_cancel->price))) {
      diff.kind = DiffKind::Cancel;
      std::ostringstream out;
      out << "cancels disagree at command " << i << "\n";
      out << "production events:\n" << FormatEvents(production_events);
      out << "reference events:\n" << FormatEvents(reference_events);
      diff.detail = out.str();
      return diff;
    }

    if (production_snap != reference_snap) {
      diff.kind = DiffKind::Snapshot;
      std::ostringstream out;
      out << "snapshots disagree at command " << i << "\n";
      out << "production:\n" << FormatSnapshot(production_snap);
      out << "reference:\n" << FormatSnapshot(reference_snap);
      diff.detail = out.str();
      return diff;
    }

    if (const char* problem = InvariantFailure(production_snap);
        problem != nullptr) {
      diff.kind = DiffKind::Invariant;
      std::ostringstream out;
      out << "invariant failed at command " << i << ": " << problem << "\n";
      out << "production:\n" << FormatSnapshot(production_snap);
      diff.detail = out.str();
      return diff;
    }
    if (const char* problem = InvariantFailure(reference_snap); problem != nullptr) {
      diff.kind = DiffKind::Invariant;
      std::ostringstream out;
      out << "invariant failed at command " << i << ": " << problem << "\n";
      out << "reference:\n" << FormatSnapshot(reference_snap);
      diff.detail = out.str();
      return diff;
    }
  }

  diff.kind = DiffKind::None;
  return diff;
}

void RequireAgree(const char* label, const std::vector<lob::NewOrder>& history,
                  const ObservableSnapshot& production,
                  const ObservableSnapshot& reference) {
  if (production == reference) {
    return;
  }
  std::cerr << "books disagree: " << label << "\n";
  std::cerr << FormatHistory(history);
  std::cerr << "production:\n" << FormatSnapshot(production);
  std::cerr << "reference:\n" << FormatSnapshot(reference);
  std::abort();
}

void RequireStream(const lob::test::Commands& commands) {
  const StreamDiff diff = FirstDiff(commands);
  if (diff.kind == DiffKind::None) {
    return;
  }
  std::cerr << "books disagree\n";
  std::cerr << FormatHistory(commands);
  std::cerr << diff.detail;
  std::abort();
}

void FailWithHistory(const char* label, const std::vector<lob::NewOrder>& history) {
  std::cerr << label << "\n";
  std::cerr << FormatHistory(history);
  std::abort();
}

void FailWithHistory(const char* label, const lob::test::Commands& history) {
  std::cerr << label << "\n";
  std::cerr << FormatHistory(history);
  std::abort();
}

lob::test::Commands Minimize(const lob::test::Commands& commands) {
  if (commands.empty() || FirstDiff(commands).kind == DiffKind::None) {
    return commands;
  }

  const DiffKind kind = FirstDiff(commands).kind;
  lob::test::Commands best = commands;

  std::size_t lo = 1;
  std::size_t hi = best.size();
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    lob::test::Commands prefix(best.begin(), best.begin() + static_cast<std::ptrdiff_t>(mid));
    const StreamDiff diff = FirstDiff(prefix);
    if (diff.kind != DiffKind::None) {
      hi = mid;
    } else {
      lo = mid + 1;
    }
  }
  best = lob::test::Commands(best.begin(), best.begin() + static_cast<std::ptrdiff_t>(lo));

  bool shrunk = true;
  while (shrunk) {
    shrunk = false;
    for (std::size_t i = 0; i < best.size(); ++i) {
      lob::test::Commands candidate;
      candidate.reserve(best.size() - 1);
      for (std::size_t j = 0; j < best.size(); ++j) {
        if (j != i) {
          candidate.push_back(best[j]);
        }
      }
      const StreamDiff diff = FirstDiff(candidate);
      if (diff.kind == kind) {
        best = std::move(candidate);
        shrunk = true;
        break;
      }
    }
  }
  return best;
}

void PersistFailure(std::uint32_t seed, const lob::test::Commands& commands,
                    const StreamDiff& diff) {
  const lob::test::Commands minimized = Minimize(commands);
  std::ostringstream body;
  body << "seed=" << seed << "\n";
  body << "original_commands=" << commands.size() << "\n";
  body << "minimized_commands=" << minimized.size() << "\n";
  body << "diff_kind=" << static_cast<int>(diff.kind) << "\n";
  body << FormatHistory(minimized);
  body << diff.detail;
  if (diff.kind != FirstDiff(minimized).kind) {
    body << FirstDiff(minimized).detail;
  }
  std::cerr << body.str();
  std::ofstream out("failing_seed.txt");
  if (out) {
    out << body.str();
  }
}

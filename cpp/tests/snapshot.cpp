#include "snapshot.hpp"

#include <iostream>
#include <sstream>
#include <cstdlib>

namespace {

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

}  // namespace

bool operator==(const ObservableSnapshot& left, const ObservableSnapshot& right) {
  return EqualLevel(left.bid_top, right.bid_top) &&
         EqualLevel(left.ask_top, right.ask_top) &&
         EqualLevels(left.bids, right.bids) && EqualLevels(left.asks, right.asks) &&
         left.live == right.live;
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

void FailWithHistory(const char* label, const std::vector<lob::NewOrder>& history) {
  std::cerr << label << "\n";
  std::cerr << FormatHistory(history);
  std::abort();
}

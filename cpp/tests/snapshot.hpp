#pragma once

#include "command.hpp"
#include "lob/order_book.hpp"
#include "reference_book.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct ObservableSnapshot {
  lob::LevelSnapshot bid_top{};
  lob::LevelSnapshot ask_top{};
  std::vector<lob::LevelSnapshot> bids;
  std::vector<lob::LevelSnapshot> asks;
  std::vector<std::pair<lob::OrderId, lob::Quantity>> live;
};

struct TradeFact {
  lob::OrderId maker{};
  lob::OrderId taker{};
  lob::PriceTicks price{};
  lob::Quantity quantity{};
};

bool operator==(const ObservableSnapshot& left, const ObservableSnapshot& right);
bool operator==(const TradeFact& left, const TradeFact& right);

ObservableSnapshot Capture(lob::OrderBook& book, const std::vector<lob::OrderId>& ids,
                           std::size_t depth);
ObservableSnapshot Capture(const lob::ref::ReferenceBook& book,
                           const std::vector<lob::OrderId>& ids, std::size_t depth);

std::string FormatHistory(const std::vector<lob::NewOrder>& history);
std::string FormatHistory(const lob::test::Commands& history);
std::string FormatSnapshot(const ObservableSnapshot& snapshot);
std::string FormatEvents(const std::vector<lob::Event>& events);
std::string FormatTrades(const std::vector<TradeFact>& trades);

bool BookIsUncrossed(const ObservableSnapshot& snapshot);
std::vector<TradeFact> TradesFrom(const std::vector<lob::Event>& events,
                                  lob::OrderId taker);

enum class DiffKind { None, Reject, Trade, Cancel, Snapshot, Crossed };

struct StreamDiff {
  DiffKind kind = DiffKind::None;
  std::size_t command_index = 0;
  std::string detail;
};

StreamDiff FirstDiff(const lob::test::Commands& commands);

void RequireAgree(const char* label, const std::vector<lob::NewOrder>& history,
                  const ObservableSnapshot& production,
                  const ObservableSnapshot& reference);
void RequireStream(const lob::test::Commands& commands);
void FailWithHistory(const char* label, const std::vector<lob::NewOrder>& history);
void FailWithHistory(const char* label, const lob::test::Commands& history);

lob::test::Commands Minimize(const lob::test::Commands& commands);
void PersistFailure(std::uint32_t seed, const lob::test::Commands& commands,
                    const StreamDiff& diff);

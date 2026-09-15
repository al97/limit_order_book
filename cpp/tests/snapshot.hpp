#pragma once

#include "lob/order_book.hpp"
#include "reference_book.hpp"

#include <cstddef>
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

bool operator==(const ObservableSnapshot& left, const ObservableSnapshot& right);

ObservableSnapshot Capture(lob::OrderBook& book, const std::vector<lob::OrderId>& ids,
                           std::size_t depth);
ObservableSnapshot Capture(const lob::ref::ReferenceBook& book,
                           const std::vector<lob::OrderId>& ids, std::size_t depth);

std::string FormatHistory(const std::vector<lob::NewOrder>& history);
std::string FormatSnapshot(const ObservableSnapshot& snapshot);

void RequireAgree(const char* label, const std::vector<lob::NewOrder>& history,
                  const ObservableSnapshot& production,
                  const ObservableSnapshot& reference);
void FailWithHistory(const char* label, const std::vector<lob::NewOrder>& history);

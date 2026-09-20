#include "lob/order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifndef LOB_BENCH_BUILD_TYPE
#define LOB_BENCH_BUILD_TYPE "unknown"
#endif

namespace {

using Clock = std::chrono::steady_clock;

struct Stats {
  std::size_t n = 0;
  double min_ns = 0;
  double p50_ns = 0;
  double p99_ns = 0;
  double max_ns = 0;
  double mean_ns = 0;
};

struct Workload {
  const char* name;
};

double Rank(std::vector<double> values, int percent) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  return values[(values.size() - 1) * static_cast<std::size_t>(percent) / 100];
}

std::string CpuName() {
#ifdef __APPLE__
  char brand[256];
  std::size_t size = sizeof(brand);
  if (sysctlbyname("machdep.cpu.brand_string", brand, &size, nullptr, 0) == 0) {
    return std::string(brand, size - 1);
  }
#endif
#ifdef __linux__
  std::ifstream cpuinfo("/proc/cpuinfo");
  std::string line;
  while (std::getline(cpuinfo, line)) {
    const std::string key = "model name";
    if (line.compare(0, key.size(), key) == 0) {
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        std::size_t i = colon + 1;
        while (i < line.size() && line[i] == ' ') {
          ++i;
        }
        return line.substr(i);
      }
    }
  }
#endif
  return "unknown";
}

std::string Compiler() {
#if defined(__clang__)
  return std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
  return std::string("GCC ") + __VERSION__;
#else
  return "unknown";
#endif
}

Stats Summarize(std::vector<double> samples) {
  Stats stats;
  if (samples.empty()) {
    return stats;
  }
  std::sort(samples.begin(), samples.end());
  stats.n = samples.size();
  stats.min_ns = samples.front();
  stats.max_ns = samples.back();
  stats.p50_ns = samples[(stats.n - 1) * 50 / 100];
  stats.p99_ns = samples[(stats.n - 1) * 99 / 100];
  stats.mean_ns = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(stats.n);
  return stats;
}

void Touch(const std::vector<lob::Event>& events, volatile std::size_t& sink) {
  sink += events.size();
}

lob::NewOrder Gtc(lob::OrderId id, lob::Side side, lob::PriceTicks price,
                  lob::Quantity quantity) {
  return {id, side, price, quantity, lob::TimeInForce::GTC};
}

Stats BenchRest(std::size_t warmup, std::size_t samples, volatile std::size_t& sink) {
  lob::OrderBook book;
  lob::OrderId id = 1;
  std::vector<double> ns;
  ns.reserve(samples);
  for (std::size_t i = 0; i < warmup + samples; ++i) {
    const auto start = Clock::now();
    const std::vector<lob::Event> events =
        book.Submit(Gtc(id++, lob::Side::Buy, 100, 1));
    const auto elapsed = Clock::now() - start;
    Touch(events, sink);
    if (i >= warmup) {
      ns.push_back(std::chrono::duration<double, std::nano>(elapsed).count());
    }
  }
  return Summarize(std::move(ns));
}

Stats BenchCancel(std::size_t fixture, volatile std::size_t& sink) {
  lob::OrderBook book;
  std::vector<lob::OrderId> live;
  live.reserve(fixture);
  for (std::size_t i = 0; i < fixture; ++i) {
    const lob::OrderId id = static_cast<lob::OrderId>(i + 1);
    Touch(book.Submit(Gtc(id, lob::Side::Buy, 100, 1)), sink);
    live.push_back(id);
  }
  std::mt19937 rng(1);
  std::shuffle(live.begin(), live.end(), rng);
  std::vector<double> ns;
  ns.reserve(live.size());
  for (const lob::OrderId id : live) {
    const auto start = Clock::now();
    const std::vector<lob::Event> events = book.Cancel(id);
    const auto elapsed = Clock::now() - start;
    Touch(events, sink);
    ns.push_back(std::chrono::duration<double, std::nano>(elapsed).count());
  }
  return Summarize(std::move(ns));
}

Stats BenchTake(std::size_t makers, std::size_t samples, volatile std::size_t& sink) {
  lob::OrderBook book;
  lob::OrderId id = 1;
  std::vector<double> ns;
  ns.reserve(samples);
  for (std::size_t i = 0; i < samples; ++i) {
    for (std::size_t m = 0; m < makers; ++m) {
      Touch(book.Submit(Gtc(id++, lob::Side::Sell, 100, 1)), sink);
    }
    const auto start = Clock::now();
    const std::vector<lob::Event> events =
        book.Submit(Gtc(id++, lob::Side::Buy, 100, static_cast<lob::Quantity>(makers)));
    const auto elapsed = Clock::now() - start;
    Touch(events, sink);
    ns.push_back(std::chrono::duration<double, std::nano>(elapsed).count());
  }
  return Summarize(std::move(ns));
}

Stats BenchReplay(std::size_t commands, volatile std::size_t& sink) {
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> percent(0, 99);
  std::uniform_int_distribution<lob::PriceTicks> price(95, 105);
  std::vector<lob::OrderId> submitted;
  lob::OrderId next_id = 1;
  lob::OrderBook book;
  std::vector<double> ns;
  ns.reserve(commands);
  for (std::size_t i = 0; i < commands; ++i) {
    const bool can_cancel = !submitted.empty();
    const int roll = percent(rng);
    const auto start = Clock::now();
    std::vector<lob::Event> events;
    if (!can_cancel || roll < 75) {
      const lob::Side side = percent(rng) < 50 ? lob::Side::Buy : lob::Side::Sell;
      events = book.Submit(Gtc(next_id, side, price(rng), 1));
      submitted.push_back(next_id);
      ++next_id;
    } else {
      const lob::OrderId id =
          submitted[static_cast<std::size_t>(percent(rng)) % submitted.size()];
      events = book.Cancel(id);
    }
    const auto elapsed = Clock::now() - start;
    Touch(events, sink);
    ns.push_back(std::chrono::duration<double, std::nano>(elapsed).count());
  }
  return Summarize(std::move(ns));
}

std::string FormatLatency(double ns) {
  std::ostringstream out;
  if (ns >= 1000.0) {
    const double us = ns / 1000.0;
    out << std::fixed << std::setprecision(1) << us << " µs";
    return out.str();
  }
  out << static_cast<long long>(ns + 0.5) << " ns";
  return out.str();
}

void PrintAcross(const Workload& workload, const std::vector<Stats>& runs) {
  std::vector<double> p50;
  std::vector<double> p99;
  p50.reserve(runs.size());
  p99.reserve(runs.size());
  for (const Stats& stats : runs) {
    p50.push_back(stats.p50_ns);
    p99.push_back(stats.p99_ns);
  }
  std::cout << std::left << std::setw(18) << workload.name << std::right << std::setw(10)
            << FormatLatency(Rank(p50, 50)) << std::setw(10)
            << FormatLatency(Rank(p99, 50)) << "\n";
}

int RepeatsFromArgs(int argc, char** argv) {
  if (argc < 2) {
    return 10;
  }
  const int parsed = std::atoi(argv[1]);
  return parsed < 1 ? 1 : parsed;
}

}  // namespace

int main(int argc, char** argv) {
  const int repeats = RepeatsFromArgs(argc, argv);
  volatile std::size_t sink = 0;
  const std::vector<Workload> workloads = {
      {"rest a bid"},
      {"cancel"},
      {"take 1 maker"},
      {"take 10 makers"},
      {"take 100 makers"},
      {"mixed replay"},
  };
  std::vector<std::vector<Stats>> runs(workloads.size());

  for (int round = 0; round < repeats; ++round) {
    runs[0].push_back(BenchRest(1000, 20000, sink));
    runs[1].push_back(BenchCancel(10000, sink));
    runs[2].push_back(BenchTake(1, 5000, sink));
    runs[3].push_back(BenchTake(10, 2000, sink));
    runs[4].push_back(BenchTake(100, 500, sink));
    runs[5].push_back(BenchReplay(100000, sink));
  }

  std::cout << "compiler: " << Compiler() << "\n";
  std::cout << "build: " << LOB_BENCH_BUILD_TYPE << "\n";
  std::cout << "cpu: " << CpuName() << "\n";
  std::cout << "repeats: " << repeats << " (median across runs)\n";
  std::cout << "containers: std::map price levels, std::list FIFO, std::map id locator\n";
  std::cout << "clock: std::chrono::steady_clock\n";
  std::cout << "typical = p50,  tail = p99\n";
  std::cout << "\n";
  std::cout << std::left << std::setw(18) << "workload" << std::right << std::setw(10)
            << "typical" << std::setw(10) << "tail" << "\n";
  for (std::size_t i = 0; i < workloads.size(); ++i) {
    PrintAcross(workloads[i], runs[i]);
  }
  if (sink == 0) {
    return 1;
  }
  return 0;
}

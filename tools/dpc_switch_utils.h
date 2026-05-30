#ifndef DPC_TOOLS_DPC_SWITCH_UTILS_H
#define DPC_TOOLS_DPC_SWITCH_UTILS_H

// ---------------------------------------------------------------------------
// Stats (shared between recv thread and CLI thread)
// ---------------------------------------------------------------------------
#include "fmt/core.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <map>
#include <mutex>
#include <string>

struct Stats {
  using clock_t = std::chrono::high_resolution_clock;
  std::mutex mu;

  struct in_t {
    uint64_t bytes = 0;
    uint64_t pkts = 0;
    uint64_t pkts_rtx = 0;
    uint64_t pkts_ign = 0;
    std::map<std::string, uint64_t> pkts_by_src; // "host (ip)" -> packets
  } in{};
  struct out_t {
    uint64_t bytes = 0; // total bytes (unicast + multicast)
    uint64_t mcast = 0;
    uint64_t ucast = 0;
    std::map<std::string, uint64_t> pkts_by_src; // "host (ip)" -> unicast packets
  } out{};
  clock_t::time_point start = clock_t::now();

  void record_in(uint64_t len, const std::string &src, bool rtx = false) {
    std::lock_guard<std::mutex> l(mu);
    in.bytes += len;
    in.pkts++;
    in.pkts_rtx += rtx;
    in.pkts_by_src[src]++;
  }

  void record_out(uint64_t len, uint16_t receivers = 1) {
    std::lock_guard<std::mutex> l(mu);
    out.ucast += (receivers == 1);
    out.mcast += (receivers != 1);
    out.bytes += len * receivers;
  }

  void record_in_ignored(uint64_t size) {
    std::lock_guard<std::mutex> l(mu);
    in.pkts_ign++;
  }

  void clear() {
    std::lock_guard<std::mutex> l(mu);
    in.pkts_by_src.clear();
    in = {};
    out = {};
  }

  void dump() {
    std::lock_guard<std::mutex> lk(mu);
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(clock_t::now() - start).count();

    fmt::println("=== stats ===\n");
    fmt::println("uptime : {}s", secs);
    fmt::println("in:\n  packets: {} ({} rtx, {} ignored)\n  bytes  : {}", in.pkts, in.pkts_rtx, in.pkts_ign, in.bytes);
    if (!in.pkts_by_src.empty()) {
      fmt::println("    by source:\n");
      for (auto &[k, v] : in.pkts_by_src) fmt::println("    {} : {}", k, v);
    }
    fmt::println("out:\n  mcast: {}\n  ucast: {}\n  bytes: {}", out.mcast, out.ucast, out.bytes);
    if (!out.pkts_by_src.empty()) {
      fmt::println("  by source:\n");
      for (auto &[k, v] : out.pkts_by_src) fmt::println("    {} : {}", k, v);
    }
    fmt::println("");
  }
};

inline void cli_controller(Stats &stats, std::atomic<bool> &running) {
  fmt::print("controller ready. commands: help, dump, clear, stop\n> ");
  std::string line;
  while (running.load() && std::getline(std::cin, line)) {
    // trim
    size_t a = line.find_first_not_of(" \t\r\n");
    size_t b = line.find_last_not_of(" \t\r\n");
    std::string cmd = (a == std::string::npos) ? "" : line.substr(a, b - a + 1);

    if (cmd.empty()) {
      // nothing
    } else if (cmd == "help" || cmd == "?" || cmd == "h") {
      fmt::println("  help   - this\n");
      fmt::println("  stats  - print stats\n");
      fmt::println("  clear  - print stats\n");
      fmt::println("stop   - shut down the switch\n");
    } else if (cmd == "stats" or cmd == "stat" or cmd == "s") {
      stats.dump();
    } else if (cmd == "stats clear") {
      stats.clear();
      fmt::println("stats cleared\n");
    } else if (cmd == "stop" || cmd == "quit" || cmd == "exit" || cmd == "q") {
      fmt::println("stopping...\n");
      running.store(false);
      break;
    } else {
      fmt::println("unknown command: {} (try 'help')", cmd);
      // std::cout << "unknown command: " << cmd << " (try 'help')\n";
    }
    if (running.load()) fmt::print("> "); // std::cout << "> " << std::flush;
  }
  running.store(false); // EOF on stdin also stops us
}

#endif // DPC_TOOLS_DPC_SWITCH_UTILS_H

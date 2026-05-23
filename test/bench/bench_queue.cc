// bench_queue.cc
#include "dpc/util/queue.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace dpc;
using namespace std::chrono;

namespace {

struct Result {
  std::string name;
  long n;
  long per_op_ns;
  long ops_per_sec;
};

std::vector<Result> results;

void record(const std::string &name, long n, long ns) {
  results.push_back({name, n, ns / n, (n * 1'000'000'000L) / ns});
}

void bench_st_push_then_pop(int n) {
  MPSCQueue<int> q;
  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i) q.push(i);
  auto t1 = steady_clock::now();
  int x;
  for (int i = 0; i < n; ++i) q.try_pop(x);
  auto t2 = steady_clock::now();
  record("st push", n, duration_cast<nanoseconds>(t1 - t0).count());
  record("st pop", n, duration_cast<nanoseconds>(t2 - t1).count());
}

void bench_st_ping_pong(int n) {
  MPSCQueue<int> q;
  int x;
  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i) {
    q.push(i);
    q.try_pop(x);
  }
  record("st ping-pong", n, duration_cast<nanoseconds>(steady_clock::now() - t0).count());
}

void bench_spsc(int n) {
  MPSCQueue<int> q;
  std::atomic<bool> done{false};
  std::thread consumer([&] {
    int x;
    long c = 0;
    while (c < n)
      if (q.try_pop(x)) ++c;
    done.store(true);
  });
  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i) q.push(i);
  while (!done.load()) std::this_thread::yield();
  record("spsc", n, duration_cast<nanoseconds>(steady_clock::now() - t0).count());
  consumer.join();
}

void bench_mpsc(int producers, int per_producer) {
  MPSCQueue<int> q;
  int total = producers * per_producer;
  std::atomic<bool> start{false};
  std::thread consumer([&] {
    int x;
    long c = 0;
    while (c < total)
      if (q.try_pop(x)) ++c;
  });
  std::vector<std::thread> threads;
  for (int p = 0; p < producers; ++p) {
    threads.emplace_back([&] {
      while (!start.load()) std::this_thread::yield();
      for (int i = 0; i < per_producer; ++i) q.push(i);
    });
  }
  auto t0 = steady_clock::now();
  start.store(true);
  for (auto &t : threads) t.join();
  consumer.join();
  record("mpsc p=" + std::to_string(producers), total, duration_cast<nanoseconds>(steady_clock::now() - t0).count());
}

void print_results() {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << std::left << std::setw(20) << "bench" << std::right << std::setw(12) << "n" << std::setw(12)
            << "per_op(ns)" << std::setw(16) << "ops/sec" << "\n";
  std::cout << std::string(60, '-') << "\n";
  for (auto &r : results) {
    std::cout << std::left << std::setw(20) << r.name << std::right << std::setw(12) << r.n << std::setw(12)
              << r.per_op_ns << std::setw(16) << r.ops_per_sec << "\n";
  }
  std::cout << std::string(60, '=') << "\n";
}

} // namespace

int main() {
  bench_st_push_then_pop(1'000'000);
  bench_st_ping_pong(1'000'000);
  bench_spsc(1'000'000);
  bench_mpsc(2, 500'000);
  bench_mpsc(4, 250'000);
  bench_mpsc(8, 125'000);
  print_results();
  return 0;
}

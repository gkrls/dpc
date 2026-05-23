// bench_task.cc
#include "dpc/backend/noop/noop_backend.h"
#include "dpc/context.h"
#include "dpc/device.h"
#include "dpc/task.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
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

std::unique_ptr<Context> make_ctx() {
  return std::make_unique<Context>(0, 1, DeviceConfig::GenericTofino1, NoopConfig(0, 1), 30000);
}

void bench_submit_only(int n) {
  auto ctx = make_ctx();
  std::vector<uint32_t> buf(1);
  std::vector<std::shared_ptr<Task>> tasks;
  tasks.reserve(n);
  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i) tasks.push_back(ctx->AllReduceAsync(buf.data(), buf.data(), 1, DataType::U32));
  record("submit only", n, duration_cast<nanoseconds>(steady_clock::now() - t0).count());
  for (auto &t : tasks) t->wait();
}

void bench_submit_wait(int n) {
  auto ctx = make_ctx();
  std::vector<uint32_t> buf(1);
  std::vector<std::shared_ptr<Task>> tasks;
  tasks.reserve(n);
  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i) tasks.push_back(ctx->AllReduceAsync(buf.data(), buf.data(), 1, DataType::U32));
  for (auto &t : tasks) t->wait();
  record("submit + wait", n, duration_cast<nanoseconds>(steady_clock::now() - t0).count());
}

void bench_sync(int n) {
  auto ctx = make_ctx();
  std::vector<uint32_t> buf(1);
  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i) ctx->AllReduce(buf.data(), buf.data(), 1, DataType::U32);
  record("sync round-trip", n, duration_cast<nanoseconds>(steady_clock::now() - t0).count());
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
  setenv("DPC_LOG", "warn", 1);
  dpc::log::level(dpc::log::Warn);
  bench_submit_only(100'000);
  bench_submit_wait(100'000);
  bench_sync(100'000);
  print_results();
  return 0;
}

#include "dpc/backend/noop/noop_backend.h"
#include "dpc/context.h"
#include "dpc/device.h"
#include "dpc/task.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

using namespace dpc;
using namespace std::chrono;

namespace {

std::unique_ptr<Context> make_ctx() {
  return std::make_unique<Context>(0, 1, DeviceConfig::GenericTofino1, NoopConfig(0, 1), 30000);
}

std::shared_ptr<Task> make_task(Context &ctx) {
  static std::vector<uint32_t> buf(1);
  return ctx.AllReduceAsync(buf.data(), buf.data(), buf.size(), DataType::U32, ReduceOp::Sum, {});
}

void bench_throughput_async(int n) {
  auto ctx = make_ctx();
  std::vector<std::shared_ptr<Task>> tasks;
  tasks.reserve(n);
  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i) tasks.push_back(make_task(*ctx));
  for (auto &t : tasks) t->wait();
  auto elapsed = steady_clock::now() - t0;
  auto ns = duration_cast<nanoseconds>(elapsed).count();
  std::cout << "throughput async  n=" << n
            << "  total=" << ns / 1'000'000 << "ms"
            << "  per_task=" << ns / n << "ns"
            << "  ops/sec=" << (n * 1'000'000'000L) / ns << "\n";
}

void bench_throughput_sync(int n) {
  auto ctx = make_ctx();
  std::vector<uint32_t> buf(1);
  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i)
    ctx->AllReduce(buf.data(), buf.data(), buf.size(), DataType::U32, ReduceOp::Sum, {});
  auto elapsed = steady_clock::now() - t0;
  auto ns = duration_cast<nanoseconds>(elapsed).count();
  std::cout << "throughput sync   n=" << n
            << "  total=" << ns / 1'000'000 << "ms"
            << "  per_task=" << ns / n << "ns"
            << "  ops/sec=" << (n * 1'000'000'000L) / ns << "\n";
}

void bench_latency(int n) {
  auto ctx = make_ctx();
  std::vector<uint32_t> buf(1);
  std::vector<long> samples;
  samples.reserve(n);
  for (int i = 0; i < n; ++i) {
    auto t0 = steady_clock::now();
    auto task = ctx->AllReduceAsync(buf.data(), buf.data(), buf.size(), DataType::U32, ReduceOp::Sum, {});
    task->wait();
    samples.push_back(duration_cast<nanoseconds>(steady_clock::now() - t0).count());
  }
  std::sort(samples.begin(), samples.end());
  std::cout << "latency           n=" << n
            << "  p50=" << samples[n / 2] << "ns"
            << "  p99=" << samples[n * 99 / 100] << "ns"
            << "  max=" << samples.back() << "ns" << "\n";
}

} // namespace

int main() {
  bench_throughput_async(100'000);
  bench_throughput_sync(100'000);
  bench_latency(10'000);
  return 0;
}

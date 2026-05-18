// test/bench/bench_worker.cc
#include "dpc/backend/noop/noop_backend.h"
#include "dpc/device.h"
#include "dpc/task.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <vector>

using namespace dpc;
using namespace std::chrono;

namespace {

class NoopTestWorker : public BackendWorker {
public:
  NoopTestWorker() : BackendWorker(0) {}
  std::atomic<int> finished{0};

protected:
  Task::Status execute(std::shared_ptr<Task>) override { return Task::Completed; }
  void on_task_finish(std::shared_ptr<Task>, Task::Status) override { ++finished; }
};

std::shared_ptr<Task> make_task() {
  static auto ctx = std::make_unique<Context>(0, 1, DeviceConfig::GenericTofino1, NoopConfig(0, 2), 30000);
  static std::vector<uint32_t> buf(1);
  return ctx->AllReduceAsync(buf.data(), buf.data(), buf.size(), DataType::U32, ReduceOp::Sum, {});
}

void bench_throughput(int n) {
  NoopTestWorker w;
  w.start();

  auto t0 = steady_clock::now();
  for (int i = 0; i < n; ++i) w.push(make_task());
  while (w.finished.load() < n) std::this_thread::yield();
  auto elapsed = steady_clock::now() - t0;

  w.stop();
  w.join();

  auto ns = duration_cast<nanoseconds>(elapsed).count();
  std::cout << "throughput  n=" << n << "  total=" << ns / 1'000'000 << "ms" << "  per_task=" << ns / n << "ns"
            << "  ops/sec=" << (n * 1'000'000'000L) / ns << "\n";
}

void bench_latency(int n) {
  NoopTestWorker w;
  w.start();

  std::vector<long> samples;
  samples.reserve(n);
  for (int i = 0; i < n; ++i) {
    int before = w.finished.load();
    auto t0 = steady_clock::now();
    w.push(make_task());
    while (w.finished.load() == before) std::this_thread::yield();
    samples.push_back(duration_cast<nanoseconds>(steady_clock::now() - t0).count());
  }

  w.stop();
  w.join();

  std::sort(samples.begin(), samples.end());
  std::cout << "latency  n=" << n << "  p50=" << samples[n / 2] << "ns" << "  p99=" << samples[n * 99 / 100] << "ns"
            << "  max=" << samples.back() << "ns" << "\n";
}

} // namespace

int main() {
  bench_throughput(100'000);
  bench_latency(10'000);
  return 0;
}
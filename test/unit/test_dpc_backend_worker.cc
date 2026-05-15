#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "dpc/backend/worker.h"
#include "dpc/task.h"

#include "doctest/doctest.h"
#include "test_helper.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <doctest/doctest.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace dpc;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

namespace {

// A worker we can drive from tests. execute() is a counter by default, but
// can be made to block on an external cv via set_blocking().
class TestWorker : public Worker {
public:
  explicit TestWorker(uint16_t tid = 0) : Worker(tid) {}
  ~TestWorker() { stop(true); }

  std::atomic<int> started{0};
  std::atomic<int> finished{0};
  std::atomic<int> aborted{0};

  // Make execute() block until release() is called. Used to test stop
  // while a task is in flight.
  void set_blocking(bool b) { blocking_ = b; }
  void release() {
    {
      std::lock_guard<std::mutex> lock(exec_mtx_);
      released_ = true;
    }
    exec_cv_.notify_all();
  }

protected:
  Task::Status execute(std::shared_ptr<Task>) override {
    if (blocking_) {
      std::unique_lock<std::mutex> lock(exec_mtx_);
      exec_cv_.wait(lock, [this] { return released_; });
    }
    return Task::Completed;
  }
  void on_task_start(std::shared_ptr<Task>) override { ++started; }
  void on_task_finish(std::shared_ptr<Task>, Task::Status) override { ++finished; }
  void on_task_abort(std::shared_ptr<Task>) override { ++aborted; }

private:
  std::atomic<bool> blocking_{false};
  std::mutex exec_mtx_;
  std::condition_variable exec_cv_;
  bool released_ = false;
};

// Spin-wait with timeout. Returns true if predicate became true.
template <typename Pred> bool wait_for(Pred pred, std::chrono::milliseconds timeout = 1s) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(10us);
  }
  return pred();
}

std::shared_ptr<Task> make_task() {
  static auto ctx = test::MakeContext();
  return test::TaskFactory::MakeTaskNoSubmit(*ctx);
}

// Empty task suitable for pushing.
// std::shared_ptr<Task> make_task() {
//   static auto ctx = test::MakeContext();
//   int ptr = 2;
//   return ctx->AllReduceAsync(&ptr, &ptr, 1234, DataType::F32, ReduceOp::Sum, {.quantization = 1});
//   // Adapt to your Task ctor -- using whatever AllReduce-style helper exists,
//   // or a dedicated test task type.
//   // return nullptr; // <-- replace with real task factory
// }

} // namespace

// ---------------------------------------------------------------------------
// smoke
// ---------------------------------------------------------------------------

TEST_CASE("worker: single task completes") {
  TestWorker w;
  w.start();
  w.push(make_task());
  REQUIRE(wait_for([&] { return w.finished.load() == 1; }));
  CHECK(w.started.load() == 1);
  CHECK(w.finished.load() == 1);
  CHECK(w.aborted.load() == 0);
  w.stop();
  w.join();
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

// TEST_CASE("worker: push before start queues until running") {
//   TestWorker w;
//   w.push(make_task());
//   std::this_thread::sleep_for(50ms);
//   CHECK(w.started.load() == 0);
//   w.start();
//   REQUIRE(wait_for([&] { return w.finished.load() == 1; }));
//   w.stop();
//   w.join();
// }

TEST_CASE("worker: stop before start exits cleanly") {
  TestWorker w;
  w.stop();
  w.join();
  CHECK(w.started.load() == 0);
  CHECK(w.finished.load() == 0);
}

TEST_CASE("worker: stop while idle wakes the worker") {
  TestWorker w;
  w.start();
  std::this_thread::sleep_for(20ms); // let it park in phase 2
  auto t0 = std::chrono::steady_clock::now();
  w.stop();
  w.join();
  auto elapsed = std::chrono::steady_clock::now() - t0;
  CHECK(elapsed < 100ms);
}

TEST_CASE("worker: start is idempotent") {
  TestWorker w;
  w.start();
  w.start();
  w.start();
  w.push(make_task());
  REQUIRE(wait_for([&] { return w.finished.load() == 1; }));
  w.stop();
  w.join();
}

TEST_CASE("worker: stop is idempotent") {
  TestWorker w;
  w.start();
  w.stop();
  w.stop();
  w.stop();
  w.join();
}

TEST_CASE("worker: start after stop is a no-op") {
  TestWorker w;
  w.stop();
  w.start(); // should not resurrect
  std::this_thread::sleep_for(50ms);
  CHECK(w.started.load() == 0);
  w.join();
  // CHECK_THROWS(w.push(make_task()));
  // Don't call push() — it would abort the test process.
}

TEST_CASE("worker: push after stop aborts") {
  CHECK_ABORTS([] {
    TestWorker w;
    w.start();
    w.stop();
    w.push(make_task());
  });
}

// ---------------------------------------------------------------------------
// lost-wakeup race (we had this bug before)
// ---------------------------------------------------------------------------
// TEST_CASE("worker: push racing with stop, all tasks reach terminal") {
//   for (int trial = 0; trial < 100; ++trial) {
//     TestWorker w;
//     w.start();
//     std::atomic<int> pushed{0};
//     std::thread pusher([&] {
//       for (int i = 0; i < 100; ++i) {
//         try {
//           w.push(make_task());
//           pushed.fetch_add(1);
//         } catch (...) { break; } // hit assert after stop
//       }
//     });
//     std::this_thread::sleep_for(std::chrono::microseconds(trial * 5));
//     w.stop();
//     pusher.join();
//     w.join();
//     CHECK(w.finished.load() + w.aborted.load() == pushed.load());
//   }
// }

TEST_CASE("worker: multiple producers, all tasks processed") {
  for (int trial = 0; trial < 20; ++trial) {
    TestWorker w;
    w.start();
    constexpr int P = 8;
    constexpr int N = 200;
    std::vector<std::thread> producers;
    for (int p = 0; p < P; ++p) {
      producers.emplace_back([&] {
        for (int i = 0; i < N; ++i) w.push(make_task());
      });
    }
    for (auto &t : producers) t.join();
    REQUIRE(wait_for([&] { return w.finished.load() == P * N; }, 5s));
    w.stop();
    w.join();
  }
}

TEST_CASE("worker: destructor during execution is clean") {
  for (int trial = 0; trial < 50; ++trial) {
    auto w = std::make_unique<TestWorker>();
    w->start();
    for (int i = 0; i < 100; ++i) w->push(make_task());
    std::this_thread::sleep_for(std::chrono::microseconds(trial * 10));
    w.reset(); // destructor: stop + join
    // If we got here without UB, the destructor handled it.
    // TSan and ASan will scream if there's a use-after-free etc.
  }
}

TEST_CASE("worker: concurrent start and stop") {
  for (int trial = 0; trial < 200; ++trial) {
    TestWorker w;
    std::thread t1([&] { w.start(); });
    std::thread t2([&] { w.stop(); });
    t1.join();
    t2.join();
    w.join();
  }
}

TEST_CASE("worker: many concurrent pushers, no losses") {
  TestWorker w;
  w.start();

  constexpr int producers = 8;
  constexpr int per_producer = 250;
  std::vector<std::thread> threads;
  for (int p = 0; p < producers; ++p) {
    threads.emplace_back([&] {
      for (int i = 0; i < per_producer; ++i) w.push(make_task());
    });
  }
  for (auto &t : threads) t.join();

  REQUIRE(wait_for([&] { return w.finished.load() == producers * per_producer; }, 5s));
  CHECK(w.started.load() == producers * per_producer);
  CHECK(w.aborted.load() == 0);

  w.stop();
  w.join();
}

// ---------------------------------------------------------------------------
// drain
// ---------------------------------------------------------------------------

TEST_CASE("worker: stop drains pending tasks as aborted") {
  TestWorker w;
  w.set_blocking(true); // execute() blocks on first task
  w.start();

  // First task starts and blocks in execute()
  w.push(make_task());
  REQUIRE(wait_for([&] { return w.started.load() == 1; }));

  // Pile up more tasks behind it
  for (int i = 0; i < 10; ++i) w.push(make_task());

  // Stop while the first is still executing
  w.stop();
  w.release(); // let execute() return so loop exits
  w.join();

  // First task ran to completion; the other 10 should be aborted in drain
  CHECK(w.finished.load() == 1);
  CHECK(w.aborted.load() == 10);
}

TEST_CASE("worker: no tasks survive stop") {
  // Total = finished + aborted should equal pushed count.
  TestWorker w;
  w.start();
  constexpr int N = 200;
  for (int i = 0; i < N; ++i) w.push(make_task());
  w.stop();
  w.join();
  CHECK(w.finished.load() + w.aborted.load() == N);
}

// ---------------------------------------------------------------------------
// stress
// ---------------------------------------------------------------------------

TEST_CASE("worker: push/idle cycles do not leak") {
  TestWorker w;
  w.start();
  for (int i = 0; i < 5000; ++i) {
    w.push(make_task());
    REQUIRE(wait_for([&, expected = i + 1] { return w.finished.load() == expected; }, 500ms));
  }
  w.stop();
  w.join();
}
#include "dpc/util/error.h"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "dpc/backend/backend.h"
#include "dpc/backend/noop/noop_backend.h"
#include "dpc/task.h"

#include "doctest/doctest.h"
#include "test_helper.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace dpc;
using namespace dpc::test;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// ControlBackend + ControlWorker: a controllable backend for testing.
//
// - `next_status` on each worker controls what execute() returns.
// - `set_blocking(true)` makes execute() block until release() is called.
// - Counters (started/finished on worker, aborted on backend) for outcome checks.
// ---------------------------------------------------------------------------

namespace {

class ControlBackend;

class ControlWorker : public BackendWorker {
public:
  ControlWorker(uint16_t tid, ControlBackend &be);
  ~ControlWorker() override {
    DPC_CHECK(!thread_.joinable(), "ControlWorker destroyed without stop()+join()");
  }

  std::atomic<Task::Status> next_status{Task::Completed};
  std::atomic<int> started{0};
  std::atomic<int> finished{0};

  void set_blocking(bool b) { blocking_ = b; }
  void release() {
    {
      std::lock_guard<std::mutex> lock(exec_mtx_);
      released_ = true;
    }
    exec_cv_.notify_all();
  }

  void join() override {
    if (thread_.joinable()) thread_.join();
  }

protected:
  Task::Status execute(std::shared_ptr<Task>) override {
    ++started;
    if (blocking_) {
      std::unique_lock<std::mutex> lock(exec_mtx_);
      exec_cv_.wait(lock, [this] { return released_; });
    }
    auto s = next_status.load();
    ++finished;
    return s;
  }

private:
  void run() { main(); }   // no pinning in test worker

  // ControlBackend &be_;
  std::atomic<bool> blocking_{false};
  std::mutex exec_mtx_;
  std::condition_variable exec_cv_;
  bool released_ = false;
  std::thread thread_;     // MUST be last
};


class ControlBackend : public MultiworkerBackend {
public:
  ControlBackend(Context &ctx, uint16_t n_workers) : MultiworkerBackend(ctx, Backend::Noop), conf_(0, n_workers) {
    for (uint16_t i = 0; i < n_workers; ++i) workers.push_back(std::make_unique<ControlWorker>(i, *this));
  }

  using MultiworkerBackend::notify;
  using MultiworkerBackend::push;
  using MultiworkerBackend::start;
  using MultiworkerBackend::stop;

  ControlWorker &worker(size_t i) { return static_cast<ControlWorker &>(*workers[i]); }
  size_t worker_count() const { return workers.size(); }
  int aborted_total() const { return aborted_total_.load(); }

  void print(bool) const override {}
  const BackendConfig &config() const override { return conf_; }

  // Override notify publicly so the test can call it directly.
  void notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status) override {
    if (status == Task::Aborted) aborted_total_.fetch_add(1);
    MultiworkerBackend::notify(tid, task, status);
  }

private:
  NoopConfig conf_;
  std::atomic<int> aborted_total_{0};
};

ControlWorker::ControlWorker(uint16_t tid, ControlBackend &be)
    : BackendWorker(static_cast<MultiworkerBackend &>(be), tid),
      // be_(be),
      thread_(&ControlWorker::run, this) {}

template <typename Pred> bool wait_for(Pred pred, std::chrono::milliseconds timeout = 1s) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(50us);
  }
  return pred();
}

std::shared_ptr<Task> make_task(Context &ctx) { return test::TaskFactory::MakeTaskNoSubmit(ctx); }

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: start/stop clean") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 2);
  be.start();
  be.stop();
}

TEST_CASE("multiworker: start is idempotent") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 2);
  be.start();
  be.start();
  be.start();
  be.stop();
}

TEST_CASE("multiworker: stop is idempotent") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 2);
  be.start();
  be.stop();
  be.stop();
  be.stop();
}

// TEST_CASE("multiworker: push before start aborts") {
//   CHECK_ABORTS_BLOCK({
//     auto ctx = MakeContext();
//     ControlBackend be(*ctx, 2);
//     be.push(make_task(*ctx));
//   });
// }

// TEST_CASE("multiworker: push after stop aborts") {
//   CHECK_ABORTS_BLOCK({
//     auto ctx = MakeContext();
//     ControlBackend be(*ctx, 2);
//     be.start();
//     be.stop();
//     be.push(make_task(*ctx));
//   });
// }
//

TEST_CASE("multiworker: push before start sets task Aborted") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 2);
  auto t = make_task(*ctx);
  be.push(t);
  CHECK(t->getStatus() == Task::Aborted);
}

TEST_CASE("multiworker: push after stop sets task Aborted") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 2);
  be.start();
  be.stop();
  auto t = make_task(*ctx);
  be.push(t);
  CHECK(t->getStatus() == Task::Aborted);
}

TEST_CASE("multiworker: stop while idle is quick") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 2);
  be.start();
  std::this_thread::sleep_for(20ms);
  auto t0 = std::chrono::steady_clock::now();
  be.stop();
  auto elapsed = std::chrono::steady_clock::now() - t0;
  CHECK(elapsed < 100ms);
}

// ---------------------------------------------------------------------------
// Single task
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: single task reaches Completed") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 3);
  be.start();
  auto t = make_task(*ctx);
  be.push(t);
  REQUIRE(wait_for([&] { return t->isFinished(); }));
  CHECK(t->getStatus() == Task::Completed);
  be.stop();
}

TEST_CASE("multiworker: same task pushed twice aborts") {
  CHECK_ABORTS_BLOCK({
    auto ctx = MakeContext();
    ControlBackend be(*ctx, 2);
    be.start();
    auto t = make_task(*ctx);
    be.push(t);
    be.push(t);
  });
}

// ---------------------------------------------------------------------------
// Notify aggregation
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: all workers Completed -> Completed") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.start();
  auto t = make_task(*ctx);
  be.push(t);
  REQUIRE(wait_for([&] { return t->isFinished(); }));
  CHECK(t->getStatus() == Task::Completed);
  be.stop();
}

TEST_CASE("multiworker: one Failed worker -> Failed") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.worker(2).next_status = Task::Failed;
  be.start();
  auto t = make_task(*ctx);
  be.push(t);
  REQUIRE(wait_for([&] { return t->isFinished(); }));
  CHECK(t->getStatus() == Task::Failed);
  be.stop();
}

TEST_CASE("multiworker: one Aborted worker -> at least Aborted") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.worker(1).next_status = Task::Aborted;
  be.start();
  auto t = make_task(*ctx);
  be.push(t);
  REQUIRE(wait_for([&] { return t->isFinished(); }));
  CHECK(t->getStatus() >= Task::Aborted);
  be.stop();
}

TEST_CASE("multiworker: mixed Failed and Aborted -> worst wins") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.worker(0).next_status = Task::Failed;
  be.worker(3).next_status = Task::Aborted;
  be.start();
  auto t = make_task(*ctx);
  be.push(t);
  REQUIRE(wait_for([&] { return t->isFinished(); }));
  auto expected = std::max(Task::Failed, Task::Aborted);
  CHECK(t->getStatus() == expected);
  be.stop();
}

TEST_CASE("multiworker: notify for unknown task aborts") {
  CHECK_ABORTS_BLOCK({
    auto ctx = MakeContext();
    ControlBackend be(*ctx, 2);
    be.start();
    auto t = make_task(*ctx);
    be.notify(0, t, Task::Completed);
  });
}

// ---------------------------------------------------------------------------
// Multiple tasks
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: many tasks all complete") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.start();
  constexpr int N = 100;
  std::vector<std::shared_ptr<Task>> tasks;
  tasks.reserve(N);
  for (int i = 0; i < N; ++i) {
    auto t = make_task(*ctx);
    tasks.push_back(t);
    be.push(t);
  }
  for (auto &t : tasks) {
    REQUIRE(wait_for([&] { return t->isFinished(); }, 5s));
    CHECK(t->getStatus() == Task::Completed);
  }
  be.stop();
}

TEST_CASE("multiworker: concurrent pushes from N threads") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.start();
  constexpr int producers = 4;
  constexpr int per_producer = 50;
  std::vector<std::vector<std::shared_ptr<Task>>> per_thread_tasks(producers);
  std::vector<std::thread> threads;
  for (int p = 0; p < producers; ++p) {
    threads.emplace_back([&, p] {
      for (int i = 0; i < per_producer; ++i) {
        auto t = make_task(*ctx);
        per_thread_tasks[p].push_back(t);
        be.push(t);
      }
    });
  }
  for (auto &th : threads) th.join();
  for (auto &batch : per_thread_tasks)
    for (auto &t : batch) {
      REQUIRE(wait_for([&] { return t->isFinished(); }, 5s));
      CHECK(t->getStatus() == Task::Completed);
    }
  be.stop();
}

TEST_CASE("multiworker: many concurrent pushers, no losses") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.start();

  constexpr int producers = 8;
  constexpr int per_producer = 250;
  std::vector<std::shared_ptr<Task>> all_tasks;
  std::mutex all_tasks_mtx;
  std::vector<std::thread> threads;
  for (int p = 0; p < producers; ++p) {
    threads.emplace_back([&] {
      for (int i = 0; i < per_producer; ++i) {
        auto t = make_task(*ctx);
        {
          std::lock_guard<std::mutex> lock(all_tasks_mtx);
          all_tasks.push_back(t);
        }
        be.push(t);
      }
    });
  }
  for (auto &t : threads) t.join();

  for (auto &t : all_tasks) {
    REQUIRE(wait_for([&] { return t->isFinished(); }, 5s));
    CHECK(t->getStatus() == Task::Completed);
  }
  be.stop();
}

// ---------------------------------------------------------------------------
// Drain
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: stop drains pending tasks as aborted") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 1); // single worker so we can pile tasks behind a blocked one
  be.worker(0).set_blocking(true);
  be.start();

  auto first = make_task(*ctx);
  be.push(first);
  REQUIRE(wait_for([&] { return be.worker(0).started.load() == 1; }));

  for (int i = 0; i < 10; ++i) be.push(make_task(*ctx));

  std::thread stopper([&] { be.stop(); });
  std::this_thread::sleep_for(20ms);
  be.worker(0).release();
  stopper.join();

  CHECK(be.worker(0).finished.load() == 1);
  CHECK(be.aborted_total() == 10);
}

TEST_CASE("multiworker: no tasks survive stop") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 1);
  be.start();
  constexpr int N = 200;
  for (int i = 0; i < N; ++i) be.push(make_task(*ctx));
  be.stop();
  CHECK(be.worker(0).finished.load() + be.aborted_total() == N);
}

// ---------------------------------------------------------------------------
// Destruction safety
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: destruction with in-flight tasks does not hang") {
  auto ctx = MakeContext();
  {
    ControlBackend be(*ctx, 4);
    be.start();
    for (int i = 0; i < 50; ++i) be.push(make_task(*ctx));
  }
}

TEST_CASE("multiworker: destructor during execution is clean") {
  auto ctx = MakeContext();
  for (int trial = 0; trial < 50; ++trial) {
    auto be = std::make_unique<ControlBackend>(*ctx, 2);
    be->start();
    for (int i = 0; i < 100; ++i) be->push(make_task(*ctx));
    std::this_thread::sleep_for(std::chrono::microseconds(trial * 10));
    be.reset();
  }
}

// ---------------------------------------------------------------------------
// Stress
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: push/idle cycles do not leak") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 1);
  be.start();
  for (int i = 0; i < 5000; ++i) {
    auto t = make_task(*ctx);
    be.push(t);
    REQUIRE(wait_for([&] { return t->isFinished(); }, 500ms));
  }
  be.stop();
}

TEST_CASE("multiworker: concurrent start and stop") {
  auto ctx = MakeContext();
  for (int trial = 0; trial < 200; ++trial) {
    ControlBackend be(*ctx, 2);
    std::thread t1([&] { be.start(); });
    std::thread t2([&] { be.stop(); });
    t1.join();
    t2.join();
  }
}

TEST_CASE("multiworker: concurrent push and stop") {
  for (int trial = 0; trial < 200; ++trial) {
    auto ctx = MakeContext();
    ControlBackend be(*ctx, 4);
    be.start();

    std::atomic<bool> stop_flag{false};
    std::thread pusher([&] {
      while (!stop_flag.load()) { be.push(make_task(*ctx)); }
    });

    std::this_thread::sleep_for(std::chrono::microseconds(trial * 10));
    be.stop();
    stop_flag.store(true);
    pusher.join();
  }
}

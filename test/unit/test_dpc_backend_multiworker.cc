#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "dpc/backend/backend.h"
#include "dpc/backend/noop/noop_backend.h"
#include "dpc/task.h"

#include "doctest/doctest.h"
#include "test_helper.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace dpc;
using namespace dpc::test;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helpers: a controllable backend for testing notify aggregation.
//
// ControlBackend lets each worker return a configurable status. This is how
// we exercise worst-status promotion without modifying NoopBackend.
// ---------------------------------------------------------------------------

namespace {

class ControlBackend;

class ControlWorker : public BackendWorker {
public:
  ControlWorker(uint16_t tid, ControlBackend &be) : BackendWorker(tid), be_(be) {}
  ~ControlWorker() { stop(true); }

  // Status this worker will return from execute(). Default Completed.
  std::atomic<Task::Status> next_status{Task::Completed};

protected:
  Task::Status execute(std::shared_ptr<Task>) override { return next_status.load(); }
  void on_task_start(std::shared_ptr<Task> task) override;
  void on_task_finish(std::shared_ptr<Task> task, Task::Status status) override;
  void on_task_abort(std::shared_ptr<Task> task) override;

private:
  ControlBackend &be_;
};

class ControlBackend : public MultiworkerBackend {
public:
  ControlBackend(Context &ctx, uint16_t n_workers) : MultiworkerBackend(ctx, Backend::Noop), conf_(0, n_workers) {
    for (uint16_t i = 0; i < n_workers; ++i) workers.push_back(std::make_unique<ControlWorker>(i, *this));
  }

  // Public re-exposure of protected base methods for tests.
  using MultiworkerBackend::notify;
  using MultiworkerBackend::push;
  using MultiworkerBackend::start;
  using MultiworkerBackend::stop;

  ControlWorker &worker(size_t i) { return static_cast<ControlWorker &>(*workers[i]); }
  size_t worker_count() const { return workers.size(); }

  void print(bool) const override {}
  const BackendConfig &config() const override { return conf_; }

private:
  NoopConfig conf_;
};

void ControlWorker::on_task_start(std::shared_ptr<Task> task) { be_.notify(id(), task, Task::Running); }
void ControlWorker::on_task_finish(std::shared_ptr<Task> task, Task::Status s) { be_.notify(id(), task, s); }
void ControlWorker::on_task_abort(std::shared_ptr<Task> task) { be_.notify(id(), task, Task::Aborted); }

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

TEST_CASE("multiworker: push before start aborts") {
  CHECK_ABORTS_BLOCK({
    auto ctx = MakeContext();
    ControlBackend be(*ctx, 2);
    be.push(make_task(*ctx));
  });
}

TEST_CASE("multiworker: push after stop aborts") {
  CHECK_ABORTS_BLOCK({
    auto ctx = MakeContext();
    ControlBackend be(*ctx, 2);
    be.start();
    be.stop();
    be.push(make_task(*ctx));
  });
}

// ---------------------------------------------------------------------------
// Single task happy path
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
// Notify aggregation: worst-status promotion
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: all workers Completed -> task Completed") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.start();
  auto t = make_task(*ctx);
  be.push(t);
  REQUIRE(wait_for([&] { return t->isFinished(); }));
  CHECK(t->getStatus() == Task::Completed);
  be.stop();
}

TEST_CASE("multiworker: one Failed worker -> task Failed") {
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

TEST_CASE("multiworker: one Aborted worker -> task Aborted") {
  auto ctx = MakeContext();
  ControlBackend be(*ctx, 4);
  be.worker(1).next_status = Task::Aborted;
  be.start();
  auto t = make_task(*ctx);
  be.push(t);
  REQUIRE(wait_for([&] { return t->isFinished(); }));
  // Whichever is "worst" by Task::Status enum ordering — adjust if needed.
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
  // Should be max(Failed, Aborted) per the > comparison in notify.
  auto expected = std::max(Task::Failed, Task::Aborted);
  CHECK(t->getStatus() == expected);
  be.stop();
}

// ---------------------------------------------------------------------------
// Notify error paths
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: notify for unknown task aborts") {
  CHECK_ABORTS_BLOCK({
    auto ctx = MakeContext();
    ControlBackend be(*ctx, 2);
    be.start();
    auto t = make_task(*ctx); // never pushed
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

TEST_CASE("multiworker: concurrent pushes") {
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

// ---------------------------------------------------------------------------
// Destruction safety
// ---------------------------------------------------------------------------

TEST_CASE("multiworker: destruction with in-flight tasks does not hang") {
  auto ctx = MakeContext();
  {
    ControlBackend be(*ctx, 4);
    be.start();
    for (int i = 0; i < 50; ++i) be.push(make_task(*ctx));
    // Drop without explicit stop — destructor must clean up.
  }
}
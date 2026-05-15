#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "test_helper.h"

#include <chrono>
#include <thread>
#include <vector>

using namespace dpc;
using namespace dpc::test;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// status queries after construction
// ---------------------------------------------------------------------------

TEST_CASE("Task: fresh task is not finished") {
  auto ctx = MakeContext(/*op_ms=*/100);
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);

  CHECK_FALSE(task->isFinished());
  CHECK_FALSE(task->isCompleted());
  CHECK_FALSE(task->isAborted());
  CHECK_FALSE(task->isFailed());

  task->wait();
}

TEST_CASE("Task: completed task reports correct status") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  task->wait();

  CHECK(task->isFinished());
  CHECK(task->isCompleted());
  CHECK_FALSE(task->isAborted());
  CHECK_FALSE(task->isFailed());
  CHECK_FALSE(task->isRunning());
  CHECK(task->getStatus() == Task::Completed);
}

// ---------------------------------------------------------------------------
// wait variants
// ---------------------------------------------------------------------------

TEST_CASE("wait: blocks until terminal") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  auto status = task->wait();
  CHECK(status == Task::Completed);
  CHECK(task->isFinished());
}

TEST_CASE("wait: returns when task completes within timeout") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  auto status = task->wait(1000ms);
  CHECK(status == Task::Completed);
}

TEST_CASE("wait: returns even if task does not finish in time") {
  auto ctx = MakeContext(/*op_ms=*/500);
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);

  auto t0 = std::chrono::steady_clock::now();
  task->wait(50ms);
  auto elapsed = std::chrono::steady_clock::now() - t0;

  CHECK(elapsed < 300ms);

  task->wait();
}

TEST_CASE("wait: on already-completed task returns immediately") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  task->wait();

  auto t0 = std::chrono::steady_clock::now();
  auto status = task->wait();
  auto elapsed = std::chrono::steady_clock::now() - t0;

  CHECK(status == Task::Completed);
  CHECK(elapsed < 10ms);
}

// ---------------------------------------------------------------------------
// terminal stickiness
// ---------------------------------------------------------------------------

TEST_CASE("Task: status remains terminal") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  task->wait();
  auto status_after_wait = task->getStatus();

  std::this_thread::sleep_for(50ms);
  CHECK(task->getStatus() == status_after_wait);
  CHECK(task->isFinished());
}

// ---------------------------------------------------------------------------
// status string
// ---------------------------------------------------------------------------

TEST_CASE("getStatusString: returns expected names") {
  CHECK(Task::getStatusString(Task::Created) == "Created");
  CHECK(Task::getStatusString(Task::Submitted) == "Submitted");
  CHECK(Task::getStatusString(Task::Running) == "Running");
  CHECK(Task::getStatusString(Task::Completed) == "Completed");
  CHECK(Task::getStatusString(Task::Aborted) == "Aborted");
  CHECK(Task::getStatusString(Task::Failed) == "Failed");
}

TEST_CASE("getStatusString: instance matches getStatus") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  task->wait();
  CHECK(task->getStatusString() == Task::getStatusString(task->getStatus()));
}

// ---------------------------------------------------------------------------
// type & collective queries
// ---------------------------------------------------------------------------

TEST_CASE("Task: type queries reflect DataType") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  CHECK(task->isInteger());
  CHECK(task->isUnsigned());
  CHECK_FALSE(task->isSigned());
  CHECK_FALSE(task->isFloatingPoint());
  task->wait();
}

TEST_CASE("Task: collective queries reflect Collective") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  CHECK(task->isAllReduce());
  CHECK(task->isReduction());
  CHECK_FALSE(task->isAllGather());
  CHECK_FALSE(task->isReduceScatter());
  task->wait();
}

// ---------------------------------------------------------------------------
// stats
// ---------------------------------------------------------------------------

TEST_CASE("getStats: populated after completion") {
  auto ctx = MakeContext(/*op_ms=*/10);
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  task->wait();

  auto stats = task->getStats();
  CHECK(stats["elements"] == 64);
  CHECK(stats["bytes"] == 64 * 4);
  CHECK(stats["time_ms"] >= 0.0f);
}
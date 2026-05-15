#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "test_helper.h"

#include <vector>

using namespace dpc;
using namespace dpc::test;

// ---------------------------------------------------------------------------
// async submit
// ---------------------------------------------------------------------------

TEST_CASE("AllReduceAsync: returns a valid task") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  REQUIRE(task);
  CHECK(task->id > 0);
  CHECK(task->isAllReduce());
}

TEST_CASE("AllReduceAsync: task eventually completes") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  CHECK(task->wait() == Task::Completed);
  CHECK(task->isCompleted());
  CHECK(task->isFinished());
}

TEST_CASE("AllReduce: returns Completed") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto status = ctx->AllReduce(data.data(), data.data(), data.size(), DataType::U32);
  CHECK(status == Task::Completed);
}

// ---------------------------------------------------------------------------
// multiple submits
// ---------------------------------------------------------------------------

TEST_CASE("AllReduceAsync: multiple tasks all complete") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);

  std::vector<std::shared_ptr<Task>> tasks;
  for (int i = 0; i < 10; ++i) {
    tasks.push_back(ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32));
  }

  for (auto &t : tasks) {
    CHECK(t->wait() == Task::Completed);
  }
}

TEST_CASE("AllReduceAsync: task ids are unique and monotonic") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);

  auto t1 = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  auto t2 = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  auto t3 = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);

  CHECK(t1->id < t2->id);
  CHECK(t2->id < t3->id);

  // Wait so they all release before context dies
  t1->wait(); t2->wait(); t3->wait();
}

// ---------------------------------------------------------------------------
// shutdown / pre-abort
// ---------------------------------------------------------------------------

TEST_CASE("Context: dropping aborts unfinished tasks") {
  std::shared_ptr<Task> task;
  {
    // Slow tasks so we can drop ctx before completion
    auto ctx = MakeContext(/*op_ms=*/50, /*threads=*/2);
    std::vector<uint32_t> data(64);
    task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  }
  // Context dropped; task must reach a terminal state, not hang
  auto status = task->wait();
  CHECK(task->isFinished());
  CHECK((status == Task::Aborted || status == Task::Completed));
}

TEST_CASE("Context: dropping with many tasks reaches terminal") {
  std::vector<std::shared_ptr<Task>> tasks;
  {
    auto ctx = MakeContext(/*op_ms=*/50, /*threads=*/2);
    std::vector<uint32_t> data(64);
    for (int i = 0; i < 20; ++i) {
      tasks.push_back(ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32));
    }
  }
  for (auto &t : tasks) {
    CHECK(t->isFinished());
  }
}
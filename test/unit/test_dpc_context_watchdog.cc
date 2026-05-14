// test/unit/test_dpc_context_watchdog.cc
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "test_helper.h"

#include <chrono>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace dpc;
using namespace dpc::test;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

// Run `body` in a forked subprocess. Returns the child's exit code.
// If the child was killed by a signal, returns -signal.
template <typename F> static int run_in_subprocess(F body) {
  pid_t pid = fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    // Child
    body();
    _exit(0); // body returned normally
  }

  // Parent: wait for child
  int status = 0;
  pid_t r = waitpid(pid, &status, 0);
  REQUIRE(r == pid);

  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return -WTERMSIG(status);
  return -1;
}

// ---------------------------------------------------------------------------
// normal operation: watchdog stays out of the way
// ---------------------------------------------------------------------------

TEST_CASE("does not fire when tasks complete within timeout") {
  auto ctx = MakeContext(/*op_ms=*/10, /*threads=*/2, /*timeout_ms=*/500);
  std::vector<uint32_t> data(64);
  for (int i = 0; i < 5; ++i) {
    auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
    CHECK(task->wait() == Task::Completed);
  }
}

TEST_CASE("zero timeout disables watchdog") {
  auto ctx = MakeContext(/*op_ms=*/100, /*threads=*/2, /*timeout_ms=*/0);
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  CHECK(task->wait() == Task::Completed);
}

// ---------------------------------------------------------------------------
// firing: watchdog kills the process
// ---------------------------------------------------------------------------

TEST_CASE("fires when task overruns timeout") {
  int rc = run_in_subprocess([] {
    auto ctx = MakeContext(/*op_ms=*/500, /*threads=*/2, /*timeout_ms=*/50);
    std::vector<uint32_t> data(64);
    auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
    task->wait();
  });
  INFO("rc=" << rc);
  CHECK(rc == 1);
}

TEST_CASE("fires during shutdown if backend hangs") {
  int rc = run_in_subprocess([] {
    auto ctx = MakeContext(/*op_ms=*/500, /*threads=*/2, /*timeout_ms=*/50);
    std::vector<uint32_t> data(64);
    ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
  });
  INFO("rc=" << rc);
  CHECK(rc == 1);
}

TEST_CASE("does not fire if task completes just before timeout") {
  // Tight but achievable: op_ms=10, timeout=200ms.
  // Tasks should complete, child should exit normally with 0.
  int rc = run_in_subprocess([] {
    auto ctx = MakeContext(/*op_ms=*/10, /*threads=*/2, /*timeout_ms=*/200);
    std::vector<uint32_t> data(64);
    for (int i = 0; i < 5; ++i) {
      auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32);
      task->wait();
    }
  });
  CHECK(rc == 0);
}
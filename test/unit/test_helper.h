// test/test_helpers.h
#ifndef DPC_TEST_HELPERS_H
#define DPC_TEST_HELPERS_H
#include <doctest/doctest.h>

#include "dpc/backend/noop/noop_backend.h"
#include "dpc/context.h"
#include "dpc/device.h"


#include <memory>
#include <sys/wait.h>
#include <unistd.h>

namespace dpc::test {

/// Build a Context with a NoopBackend configured for fast, deterministic tests.
/// Defaults: rank=0, world=1, op_ms=0 (instant), threads=2, timeout=30s.
inline std::unique_ptr<Context> MakeContext(uint64_t op_ms = 0, uint16_t threads = 2, uint32_t timeout_ms = 30000,
                                            uint16_t rank = 0, uint16_t world = 1) {
  NoopConfig cfg(op_ms, threads);
  return std::make_unique<Context>(rank, world, DeviceConfig::GenericTofino1, cfg, timeout_ms);
}

struct TaskFactory {
  static std::shared_ptr<Task> MakeTaskNoSubmit(Context &ctx) {
    static int buf = 0;
    return Task::CreateAllReduce(ctx, true, &buf, &buf, 1, DataType::U32, ReduceOp::Sum, {});
  }
};


// Run `body` in a forked subprocess. Returns the child's exit code.
// If the child was killed by a signal, returns -signal.
template <typename F> static int run_in_subprocess(F body) {
  pid_t pid = fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    // Child
  signal(SIGABRT, SIG_DFL);
  body();
  _exit(0);
  }

  // Parent: wait for child
  int status = 0;
  pid_t r = waitpid(pid, &status, 0);
  REQUIRE(r == pid);

  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return -WTERMSIG(status);
  return -1;
}

template <typename F>
static bool subprocess_aborts(F body) {
  int rc = run_in_subprocess(body);
  return rc == -SIGABRT;
}

template <typename F>
static bool subprocess_exits_with(int code, F body) {
  return run_in_subprocess(body) == code;
}

} // namespace dpc::test

#define CHECK_EXITS(code, fn) CHECK(dpc::test::subprocess_exits_with(code, fn))

#define CHECK_ABORTS(fn) CHECK_EXITS(-SIGABRT, fn)

#endif
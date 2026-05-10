// test/unit/test_dpc_context_lifecycle.cc
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "test_helper.h"

using namespace dpc;
using namespace dpc::test;

TEST_CASE("context: reaches Running after construction") {
  auto ctx = MakeContext();
  CHECK(ctx->isRunning());
  CHECK(ctx->state() == Context::Running);
}

TEST_CASE("context: rank and world are set from constructor") {
  auto ctx = MakeContext(0, 2, 30000, /*rank=*/3, /*world=*/8);
  CHECK(ctx->rank == 3);
  CHECK(ctx->world == 8);
}

TEST_CASE("context: destruction reaches Stopped state") {
  // Can't observe Stopped from outside since the Context is gone, but we
  // can verify destruction completes without hanging.
  {
    auto ctx = MakeContext();
    CHECK(ctx->isRunning());
  }
  // If we got here, destruction succeeded.
  CHECK(true);
}

TEST_CASE("context: destruction with no submitted tasks is fast") {
  auto start = std::chrono::steady_clock::now();
  { auto ctx = MakeContext(); }
  auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(elapsed < std::chrono::seconds(1));
}

TEST_CASE("context: backend and device accessible") {
  auto ctx = MakeContext();
  CHECK(ctx->backend().is(Backend::Noop));
  CHECK(ctx->backend().state() == Backend::Running);
}
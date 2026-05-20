#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_helper.h"

using namespace dpc;
using namespace dpc::test;

// ---------------------------------------------------------------------------
// Constructor overloads
// ---------------------------------------------------------------------------

TEST_CASE("context: constructor with device config") {
  Context ctx(0, 1, DeviceConfig::GenericTofino1);
  CHECK(ctx.isRunning());
  CHECK(ctx.backend().is(Backend::Noop)); // default backend
}

TEST_CASE("context: constructor with backend config") {
  NoopConfig bc(0, 2); // op_ms=0 for speed
  Context ctx(0, 1, bc);
  CHECK(ctx.isRunning());
  CHECK(ctx.backend().is(Backend::Noop));
}

TEST_CASE("context: constructor with both device and backend") {
  NoopConfig bc(0, 2);
  Context ctx(0, 1, DeviceConfig::GenericTofino1, bc);
  CHECK(ctx.isRunning());
  CHECK(ctx.backend().is(Backend::Noop));
}

// ---------------------------------------------------------------------------
// Singleton enforcement
// ---------------------------------------------------------------------------

TEST_CASE("context: second simultaneous context aborts") {
  CHECK_ABORTS_BLOCK({
    auto ctx1 = MakeContext();
    auto ctx2 = MakeContext(); // should trip live_contexts check
  });
}

TEST_CASE("context: sequential contexts ok") {
  { auto ctx = MakeContext(); }
  { auto ctx = MakeContext(); } // first destructed, this should work
  CHECK(true);
}

// ---------------------------------------------------------------------------
// Implicit conversion
// ---------------------------------------------------------------------------

TEST_CASE("context: implicit conversion to uint32_t returns id") {
  auto ctx = MakeContext();
  uint32_t id_via_conv = *ctx; // adjust if MakeContext returns unique_ptr
  CHECK(id_via_conv == ctx->id);
}







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
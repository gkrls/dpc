// test/unit/test_dpc_api_validation.cc
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "test_helper.h"

#include <stdexcept>
#include <vector>

using namespace dpc;
using namespace dpc::test;

// ---------------------------------------------------------------------------
// AllReduce: buffer / count validation
// ---------------------------------------------------------------------------

TEST_CASE("AllReduce: rejects null sendbuf") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(nullptr, data.data(), data.size(), DataType::U32),
    std::runtime_error);
}

TEST_CASE("AllReduce: rejects null recvbuf") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), nullptr, data.size(), DataType::U32),
    std::runtime_error);
}

TEST_CASE("AllReduce: rejects zero count") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), data.data(), 0, DataType::U32),
    std::runtime_error);
}

// ---------------------------------------------------------------------------
// AllReduce: reduce op validation
// ---------------------------------------------------------------------------

TEST_CASE("AllReduce: rejects unsupported reduce ops") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);

  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32, ReduceOp::Min),
    std::runtime_error);
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32, ReduceOp::Max),
    std::runtime_error);
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32, ReduceOp::Prod),
    std::runtime_error);
}

TEST_CASE("AllReduce: accepts Sum") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  auto task = ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32, ReduceOp::Sum);
  REQUIRE(task);
  task->wait();
}

// ---------------------------------------------------------------------------
// AllReduce: int vs float rules
// ---------------------------------------------------------------------------

TEST_CASE("AllReduce: integer rejects quantization") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CollectiveOptions opt;
  opt.quantization = 4;
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32, ReduceOp::Sum, opt),
    std::runtime_error);
}

TEST_CASE("AllReduce: integer rejects Avg") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::U32, ReduceOp::Avg),
    std::runtime_error);
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::I32, ReduceOp::Avg),
    std::runtime_error);
}

TEST_CASE("AllReduce: float requires quantization") {
  auto ctx = MakeContext();
  std::vector<float> data(64);
  // quantization == 0 by default
  CHECK_THROWS_AS(
    ctx->AllReduceAsync(data.data(), data.data(), data.size(), DataType::F32),
    std::runtime_error);
}

// ---------------------------------------------------------------------------
// AllGather: validation
// ---------------------------------------------------------------------------

TEST_CASE("AllGather: rejects null sendbuf") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CHECK_THROWS_AS(
    ctx->AllGatherAsync(nullptr, data.data(), data.size(), DataType::U32),
    std::runtime_error);
}

TEST_CASE("AllGather: rejects null recvbuf") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CHECK_THROWS_AS(
    ctx->AllGatherAsync(data.data(), nullptr, data.size(), DataType::U32),
    std::runtime_error);
}

TEST_CASE("AllGather: rejects quantization") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CollectiveOptions opt;
  opt.quantization = 4;
  CHECK_THROWS_AS(
    ctx->AllGatherAsync(data.data(), data.data(), data.size(), DataType::U32, opt),
    std::runtime_error);
}

TEST_CASE("AllGather: rejects pipes option") {
  auto ctx = MakeContext();
  std::vector<uint32_t> data(64);
  CollectiveOptions opt;
  opt.pipes = 2;
  CHECK_THROWS_AS(
    ctx->AllGatherAsync(data.data(), data.data(), data.size(), DataType::U32, opt),
    std::runtime_error);
}
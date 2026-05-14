// test/unit/test_config_parsing.cc
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "dpc/backend/noop/noop_backend.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

using namespace dpc;

namespace {
struct TempJsonFile {
  std::filesystem::path path;
  explicit TempJsonFile(const std::string &content) {
    path = std::filesystem::temp_directory_path() / ("dpc_test_" + std::to_string(std::random_device{}()) + ".json");
    std::ofstream(path) << content;
  }
  ~TempJsonFile() { std::filesystem::remove(path); }
  std::string str() const { return path.string(); }
};
} // namespace

// ---------------------------------------------------------------------------
// NoopConfig::fromJson — happy paths
// ---------------------------------------------------------------------------

TEST_CASE("NoopConfig: parses all fields") {
  TempJsonFile f(R"({"noop": {"op_ms": 1000, "threads": 8}})");
  auto cfg = NoopConfig::fromJson(f.str());
  CHECK(cfg.op_ms == 1000);
  CHECK(cfg.threads == 8);
}

TEST_CASE("NoopConfig: missing fields use defaults") {
  TempJsonFile f(R"({"noop": {}})");
  auto cfg = NoopConfig::fromJson(f.str());
  CHECK(cfg.op_ms == 500); // default
  CHECK(cfg.threads == 2); // default
}

TEST_CASE("NoopConfig: partial fields keep other defaults") {
  TempJsonFile f(R"({"noop": {"threads": 16}})");
  auto cfg = NoopConfig::fromJson(f.str());
  CHECK(cfg.op_ms == 500); // unchanged
  CHECK(cfg.threads == 16);
}

// ---------------------------------------------------------------------------
// NoopConfig::fromJson — error cases
// ---------------------------------------------------------------------------

TEST_CASE("NoopConfig: rejects nonexistent file") {
  CHECK_THROWS_AS(NoopConfig::fromJson("/nonexistent/path/config.json"), std::runtime_error);
}

TEST_CASE("NoopConfig: rejects malformed JSON") {
  TempJsonFile f("{not valid json");
  CHECK_THROWS_AS(NoopConfig::fromJson(f.str()), std::runtime_error);
}

TEST_CASE("NoopConfig: rejects non-object top level") {
  TempJsonFile f("[1, 2, 3]");
  CHECK_THROWS_AS(NoopConfig::fromJson(f.str()), std::runtime_error);
}

TEST_CASE("NoopConfig: rejects missing 'noop' key") {
  TempJsonFile f(R"({"dpdk": {}})");
  CHECK_THROWS_AS(NoopConfig::fromJson(f.str()), std::runtime_error);
}

TEST_CASE("NoopConfig: rejects wrong field type") {
  TempJsonFile f(R"({"noop": {"threads": "not a number"}})");
  CHECK_THROWS_AS(NoopConfig::fromJson(f.str()), std::runtime_error);
}

// ---------------------------------------------------------------------------
// BackendConfig::fromJson — dispatcher
// ---------------------------------------------------------------------------

TEST_CASE("BackendConfig: dispatches to NoopConfig") {
  TempJsonFile f(R"({"noop": {"op_ms": 200, "threads": 4}})");
  auto cfg = BackendConfig::fromJson(f.str());
  REQUIRE(cfg);
  CHECK(cfg->is(Backend::Noop));
  auto *noop = dynamic_cast<NoopConfig *>(cfg.get());
  REQUIRE(noop);
  CHECK(noop->op_ms == 200);
  CHECK(noop->threads == 4);
}

TEST_CASE("BackendConfig: rejects file with no backend key") {
  TempJsonFile f(R"({"some_other_key": {}})");
  CHECK_THROWS_AS(BackendConfig::fromJson(f.str()), std::runtime_error);
}

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "test_helper.h"


#include "dpc/backend/backend.h"
#include "dpc/backend/noop/noop_backend.h"

#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <random>

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
// Backend::name / Backend::kind — roundtrip
// ---------------------------------------------------------------------------

TEST_CASE("Backend::name returns the registered name") {
  CHECK(Backend::name(Backend::Noop) == "noop");
  CHECK(Backend::name(Backend::Sock) == "sock");
}

TEST_CASE("Backend::kind returns the registered kind") {
  CHECK(Backend::kind("noop") == Backend::Noop);
  CHECK(Backend::kind("sock") == Backend::Sock);
}

TEST_CASE("Backend::name and Backend::kind roundtrip") {
  for (auto k : {Backend::Noop, Backend::Sock}) {
    CHECK(Backend::kind(Backend::name(k)) == k);
  }
}

TEST_CASE("Backend::kind on unknown name throws") {
  CHECK_ABORTS([] { Backend::kind("not_a_backend"); });
}

TEST_CASE("Backend::get on unknown name throws") {
  CHECK_ABORTS([] { Backend::get("not_a_backend"); });
}

// ---------------------------------------------------------------------------
// BackendConfig::get — default configs
// ---------------------------------------------------------------------------

TEST_CASE("BackendConfig::get(Kind) returns the right derived type") {
  auto cfg = BackendConfig::get(Backend::Noop);
  REQUIRE(cfg);
  CHECK(cfg->is(Backend::Noop));
  auto *noop = dynamic_cast<NoopConfig *>(cfg.get());
  REQUIRE(noop);
  // Defaults
  CHECK(noop->op_ms == 500);
  CHECK(noop->threads == 2);
}

TEST_CASE("BackendConfig::get(name) returns the right derived type") {
  auto cfg = BackendConfig::get("noop");
  REQUIRE(cfg);
  CHECK(cfg->is(Backend::Noop));
}

TEST_CASE("BackendConfig::get on unknown name throws") {
  CHECK_THROWS(BackendConfig::get("not_a_backend"));
}

// ---------------------------------------------------------------------------
// BackendConfig::fromJson — dispatcher behavior
// ---------------------------------------------------------------------------

TEST_CASE("BackendConfig::fromJson dispatches to Noop") {
  TempJsonFile f(R"({"noop": {"op_ms": 200, "threads": 4}})");
  auto cfg = BackendConfig::fromJson(f.str());
  REQUIRE(cfg);
  CHECK(cfg->is(Backend::Noop));
  auto *noop = dynamic_cast<NoopConfig *>(cfg.get());
  REQUIRE(noop);
  CHECK(noop->op_ms == 200);
  CHECK(noop->threads == 4);
}

TEST_CASE("BackendConfig::fromJson picks the first recognized backend") {
  // Both keys present. File order is whatever the JSON library yields;
  // we just check it picks one of them, not a specific one.
  TempJsonFile f(R"({"noop": {"threads": 7}, "unknown_backend": {}})");
  auto cfg = BackendConfig::fromJson(f.str());
  REQUIRE(cfg);
  CHECK(cfg->is(Backend::Noop));
}

TEST_CASE("BackendConfig::fromJson throws when no recognized backend") {
  TempJsonFile f(R"({"unknown_backend": {}, "another_unknown": {}})");
  CHECK_THROWS(BackendConfig::fromJson(f.str()));
}

TEST_CASE("BackendConfig::fromJson throws on missing file") {
  CHECK_THROWS(BackendConfig::fromJson("/nonexistent/path/abc.json"));
}

TEST_CASE("BackendConfig::fromJson throws on malformed JSON") {
  TempJsonFile f("{not json");
  CHECK_THROWS(BackendConfig::fromJson(f.str()));
}

TEST_CASE("BackendConfig::fromJson throws on non-object root") {
  TempJsonFile f("[1, 2, 3]");
  CHECK_THROWS(BackendConfig::fromJson(f.str()));
}

// ---------------------------------------------------------------------------
// BackendConfig::fromJson(path, Kind) — targeted lookup
// ---------------------------------------------------------------------------

TEST_CASE("BackendConfig::fromJson(path, Kind) loads requested backend") {
  TempJsonFile f(R"({"noop": {"op_ms": 333, "threads": 5}})");
  auto cfg = BackendConfig::fromJson(f.str(), Backend::Noop);
  REQUIRE(cfg);
  CHECK(cfg->is(Backend::Noop));
  auto *noop = dynamic_cast<NoopConfig *>(cfg.get());
  REQUIRE(noop);
  CHECK(noop->op_ms == 333);
  CHECK(noop->threads == 5);
}

TEST_CASE("BackendConfig::fromJson(path, Kind) throws if section missing") {
  TempJsonFile f(R"({"sock": {}})");
  CHECK_THROWS(BackendConfig::fromJson(f.str(), Backend::Noop));
}

TEST_CASE("BackendConfig::fromJson(path, Kind) throws on missing file") {
  CHECK_THROWS(BackendConfig::fromJson("/nope.json", Backend::Noop));
}

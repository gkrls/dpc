#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "dpc/util/config.h"

#include "doctest/doctest.h"

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
// json_load
// ---------------------------------------------------------------------------

TEST_CASE("json_load: returns root object") {
  TempJsonFile f(R"({"a": 1, "b": "x"})");
  auto j = conf::json_load(f.str());
  CHECK(j["a"] == 1);
  CHECK(j["b"] == "x");
}

TEST_CASE("json_load: nonexistent file throws") {
  CHECK_THROWS_AS(conf::json_load("/nonexistent/path.json"), std::runtime_error);
}

TEST_CASE("json_load: malformed JSON throws") {
  TempJsonFile f("{not valid");
  CHECK_THROWS_AS(conf::json_load(f.str()), std::runtime_error);
}

TEST_CASE("json_load: empty file throws") {
  TempJsonFile f("");
  CHECK_THROWS_AS(conf::json_load(f.str()), std::runtime_error);
}

// ---------------------------------------------------------------------------
// json_load_at
// ---------------------------------------------------------------------------

TEST_CASE("json_load_at: empty pointer returns root") {
  TempJsonFile f(R"({"a": 1, "b": 2})");
  auto j = conf::json_load_at(f.str());
  CHECK(j["a"] == 1);
  CHECK(j["b"] == 2);
}

TEST_CASE("json_load_at: simple path returns sub-object") {
  TempJsonFile f(R"({"noop": {"threads": 4}})");
  auto j = conf::json_load_at(f.str(), "/noop");
  CHECK(j["threads"] == 4);
}

TEST_CASE("json_load_at: nested path returns nested value") {
  TempJsonFile f(R"({"a": {"b": {"c": 42}}})");
  auto j = conf::json_load_at(f.str(), "/a/b");
  CHECK(j["c"] == 42);
}

TEST_CASE("json_load_at: missing pointer throws") {
  TempJsonFile f(R"({"a": 1})");
  CHECK_THROWS_AS(conf::json_load_at(f.str(), "/missing"), std::runtime_error);
}

TEST_CASE("json_load_at: invalid pointer syntax throws") {
  TempJsonFile f(R"({"a": 1})");
  CHECK_THROWS(conf::json_load_at(f.str(), "no_leading_slash"));
}

// ---------------------------------------------------------------------------
// json_read_if_present
// ---------------------------------------------------------------------------

TEST_CASE("json_read_if_present: reads existing key") {
  nlohmann::json j = {{"x", 42}};
  int v = 0;
  CHECK(conf::json_read_if_present(v, j, "x"));
  CHECK(v == 42);
}

TEST_CASE("json_read_if_present: missing key returns false, leaves default") {
  nlohmann::json j = {{"x", 42}};
  int v = 99;
  CHECK_FALSE(conf::json_read_if_present(v, j, "y"));
  CHECK(v == 99);
}

TEST_CASE("json_read_if_present: wrong type throws") {
  nlohmann::json j = {{"x", "not a number"}};
  int v = 0;
  CHECK_THROWS_AS(conf::json_read_if_present(v, j, "x"), std::runtime_error);
}

TEST_CASE("json_read_if_present: works for strings") {
  nlohmann::json j = {{"name", "hello"}};
  std::string s;
  CHECK(conf::json_read_if_present(s, j, "name"));
  CHECK(s == "hello");
}

TEST_CASE("json_read_if_present: works for bool") {
  nlohmann::json j = {{"flag", true}};
  bool b = false;
  CHECK(conf::json_read_if_present(b, j, "flag"));
  CHECK(b == true);
}

// ---------------------------------------------------------------------------
// CONF macro
// ---------------------------------------------------------------------------

namespace {
struct DummyConfig {
  int threads = 1;
  std::string name = "default";
};
} // namespace

TEST_CASE("CONF macro: reads matching fields") {
  nlohmann::json j = {{"threads", 8}, {"name", "test"}};
  DummyConfig c;
  OPT(j, c, threads);
  OPT(j, c, name);
  CHECK(c.threads == 8);
  CHECK(c.name == "test");
}

TEST_CASE("OPT macro: missing fields keep defaults") {
  nlohmann::json j = {{"threads", 16}};
  DummyConfig c;
  OPT(j, c, threads);
  OPT(j, c, name);
  CHECK(c.threads == 16);
  CHECK(c.name == "default");
}

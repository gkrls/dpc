#include "dpc/backend/noop/noop_backend.h"
#include "dpc/util/config.h"
#include "dpc/util/env.h"
#include "dpc/util/error.h"

#include "nlohmann/json.hpp"

#include <fstream>
#include <string>

using namespace dpc;

namespace {
const auto kThreads = env::getuint({"DPC_NOOP_THREADS"});
const auto kOpMs = env::getuint({"DPC_NOOP_OP_MS"});
} // namespace

NoopConfig NoopConfig::fromJson(const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open()) DPC_ERROR("Failed to open json config '{}'", path);
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(f);
  } catch (const nlohmann::json::parse_error &e) { DPC_ERROR("Parse error in json config {}: {}", path, e.what()); }
  if (!root.is_object()) DPC_ERROR("config {} must be a json object", path);
  if (!root.contains(Backend::name(Backend::Noop))) DPC_ERROR("config '{}' does not contain 'noop'", path);

  NoopConfig c;
  conf::read_if_present(c.threads, root, "/noop/threads");
  conf::read_if_present(c.op_ms, root, "/noop/op_ms");
  return c;
}

// std::string NoopConfig::string() const {
//   // TODO: implement me
//   return "noop-config";
// }
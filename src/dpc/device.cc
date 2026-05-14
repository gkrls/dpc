#include "dpc/device.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"
#include "nlohmann/json.hpp"

#include <fstream>

using namespace dpc;

Device::Device(DeviceConfig const &conf) : conf(conf) {
  if (conf.session.pool.size == 0 || (conf.session.pool.size % 2) != 0)
    DPC_FATAL("session pool size must be non-zero and even");
}

void Device::print(bool detail) { DPC_INFO("DEV: {}", conf.name); }

namespace {
nlohmann::json parse_json(const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open()) DPC_ERROR("Failed to open json config '{}'", path);
  try {
    return nlohmann::json::parse(f);
  } catch (const nlohmann::json::parse_error &e) { DPC_ERROR("Parse error in json config {}: {}", path, e.what()); }
}
} // namespace

DeviceConfig DeviceConfig::fromJson(const std::string &path) {
  auto data = parse_json(path);
  if (!data.is_object()) DPC_ERROR("config {} must be a json object", path);

  DeviceConfig c;

  return c;
}

// Ctx:
// Dev:
// Bck:

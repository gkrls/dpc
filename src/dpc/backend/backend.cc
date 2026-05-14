#include "dpc/backend/backend.h"

#include "dpc/backend/noop/noop_backend.h"

#include <string_view>
#if DPC_DPDK_ENABLED
#include "dpc/backend/dpdk/dpdk_backend.h"
#endif

#include "dpc/util/error.h"

#include "error.h"
#include "nlohmann/json.hpp"

#include <fstream>
#include <memory>

using namespace dpc;
using nlohmann::json;

namespace {
nlohmann::json parse_json(const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open()) DPC_ERROR("Failed to open json config '{}'", path);
  try {
    return nlohmann::json::parse(f);
  } catch (const nlohmann::json::parse_error &e) { DPC_ERROR("Parse error in json config {}: {}", path, e.what()); }
}
} // namespace

std::unique_ptr<BackendConfig> BackendConfig::fromJson(const std::string &path) {
  auto data = parse_json(path);
  if (!data.is_object()) DPC_ERROR("config {} must be a json object", path);

  for (auto it = data.begin(); it != data.end(); ++it) {
    for (const auto &[kind, name] : Backend::registry) {
      if (it.key() != name) continue;
      switch (kind) {
      case Backend::Noop: return std::make_unique<NoopConfig>(NoopConfig::fromJson(path));
#if DPC_DPDK_ENABLED
      case Backend::Dpdk: return std::make_unique<DpdkConfig>(DpdkConfig::fromJson(path));
#endif
      }
    }
  }
  DPC_ERROR("config {} contains no recognized backend", path);
}

std::unique_ptr<BackendConfig> BackendConfig::fromJson(const std::string &path, Backend::Kind kind) {
  auto data = parse_json(path);
  if (!data.is_object()) DPC_ERROR("config {} must be a json object", path);

  const std::string &name = Backend::name(kind);
  if (!data.contains(name)) DPC_ERROR("config {} does not contain '{}' section", path, name);

  switch (kind) {
  case Backend::Noop: return std::make_unique<NoopConfig>(NoopConfig::fromJson(path));
#if DPC_DPDK_ENABLED
  case Backend::Dpdk: return std::make_unique<DpdkConfig>(DpdkConfig::fromJson(path));
#endif
  }
  DPC_ERROR("unhandled backend kind in {}", path);
}

std::unique_ptr<Backend> Backend::create(Context &ctx, Backend::Kind kind) {
  switch (kind) {
  case Noop: return std::unique_ptr<NoopBackend>(new NoopBackend(ctx));
#if DPC_DPDK_ENABLED
  case Dpdk: return std::unique_ptr<DpdkBackend>(new DpdkBackend(ctx));
#endif
  default: DPC_UNREACHABLE();
  }
}

std::unique_ptr<Backend> Backend::create(Context &ctx, BackendConfig const &conf) {
  if (conf.is(Backend::Noop))
    return std::unique_ptr<NoopBackend>(new NoopBackend(ctx, static_cast<const NoopConfig &>(conf)));
#if DPC_DPDK_ENABLED
  else if (conf.is(Backend::Dpdk))
    return std::unique_ptr<DpdkBackend>(new DpdkBackend(ctx, static_cast<const DpdkConfig &>(conf)));
#endif
  else DPC_ERROR("unknown backend");
  return nullptr;
}

std::string Backend::name(Backend::Kind kind) {
  for (auto &[k, n] : registry)
    if (k == kind) return n;
  DPC_FATAL("internal: unregistered backend kind");
}

Backend::Kind Backend::get(std::string_view name) {
  for (auto &[k, n] : registry)
    if (n == name) return k;
  DPC_FATAL("internal: unregistered backend name '{}'", name);
}

Backend::Kind Backend::kind(std::string_view name) { return get(name); }

// std::string Backend::getName(BackendConfig const& conf) {
//   return Backend::getName(conf.kind_);
// };

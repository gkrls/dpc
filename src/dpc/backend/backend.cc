#include "dpc/backend/backend.h"

#include "dpc/backend/noop/noop_backend.h"
#include "dpc/backend/sock/sock_backend.h"

#include <string_view>
#if DPC_DPDK_ENABLED
#include "dpc/backend/dpdk/dpdk_backend.h"
#endif
#include "dpc/util/error.h"

#include "nlohmann/json.hpp"

#include <memory>

using namespace dpc;

using nlohmann::json;

// ============= BACKEND REGISTRATION =============
const std::vector<Backend::Entry> Backend::registry = {
    {Noop, "noop", &make_backend<NoopBackend, NoopConfig>, &make_config<NoopConfig>},
    {Sock, "sock", &make_backend<SockBackend, SockConfig>, &make_config<SockConfig>},
#if DPC_DPDK_ENABLED
    {Dpdk, "dpdk", &make_backend<DpdkBackend, DpdkConfig>, &make_config<DpdkConfig>},
#endif
};
// ============= BACKEND REGISTRATION =============

std::unique_ptr<BackendConfig> BackendConfig::fromJson(const std::string &path) {
  auto data = conf::load_json(path);
  if (!data.is_object()) DPC_ERROR("config {} must be a json object", path);

  for (auto it = data.begin(); it != data.end(); ++it) {
    for (auto &e : Backend::registry) {
      if (it.key() == e.name) return e.make_config(path);
    }
  }
  DPC_ERROR("config {} contains no recognized backend", path);
}

std::unique_ptr<BackendConfig> BackendConfig::fromJson(const std::string &path, Backend::Kind kind) {
  auto data = conf::load_json(path);
  if (!data.is_object()) DPC_ERROR("config {} must be a json object", path);
  for (auto &e : Backend::registry) {
    if (e.kind != kind) continue;
    if (!data.contains(e.name)) DPC_ERROR("config {} does not contain '{}' section", path, e.name);
    return e.make_config(path);
  }
  DPC_ERROR("unhandled backend kind in {}", path);
}

std::unique_ptr<BackendConfig> BackendConfig::get(Backend::Kind kind) {
  for (auto &e : Backend::registry)
    if (e.kind == kind) return e.make_config("");
  DPC_UNREACHABLE("unhandled kind");
}

std::unique_ptr<BackendConfig> BackendConfig::get(const std::string &name) {
  for (auto &e : Backend::registry)
    if (e.name == name) return e.make_config("");
  DPC_ERROR("unknown backend {}", name);
}





std::unique_ptr<Backend> Backend::create(Context &ctx, Backend::Kind kind) {
  for (auto &e : registry)
    if (e.kind == kind) return e.make_backend(ctx, *BackendConfig::get(kind));
  DPC_UNREACHABLE("unhandled kind");
}

std::unique_ptr<Backend> Backend::create(Context &ctx, BackendConfig const &conf) {
  for (auto &e : registry)
    if (e.kind == conf.kind_) return e.make_backend(ctx, conf);
  DPC_UNREACHABLE("unhandled kind");
}

std::string Backend::name(Backend::Kind kind) {
  for (auto &e : registry)
    if (e.kind == kind) return e.name;
  DPC_FATAL("internal: unregistered backend kind");
}

Backend::Kind Backend::get(std::string_view name) {
  for (auto &e : registry)
    if (e.name == name) return e.kind;
  DPC_FATAL("internal: unregistered backend name '{}'", name);
}

Backend::Kind Backend::kind(std::string_view name) { return get(name); }

// std::string Backend::getName(BackendConfig const& conf) {
//   return Backend::getName(conf.kind_);
// };

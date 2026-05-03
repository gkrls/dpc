#include "dpc/backend/backend.h"

#include "dpc/backend/noop/noop_backend.h"
#if DPC_DPDK_ENABLED
#include "dpc/backend/dpdk/dpdk_backend.h"
#endif

#include "dpc/util/error.h"
#include <memory>

using namespace dpc;

std::shared_ptr<Backend> Backend::create(Context &ctx, Backend::Kind kind) {
  switch (kind) {
  case Noop: return std::shared_ptr<NoopBackend>(new NoopBackend(ctx));
  case Dpdk:
#if DPC_DPDK_ENABLED
    return std::make_shared<DpdkBackend>(ctx);
#else
    DPC_FATAL("DPC built without DPDK support. Rebuild with DPC_DPDK=ON");
#endif
  default: DPC_UNREACHABLE();
  }
}

std::shared_ptr<Backend> Backend::create(Context &ctx, BackendConfig const &conf) {
  if (conf.is(Backend::Noop))
    return std::shared_ptr<NoopBackend>(new NoopBackend(ctx, static_cast<const NoopConfig &>(conf)));
#if DPC_DPDK_ENABLED
  else if (conf.is(Backend::Dpdk)) return std::make_shared<DpdkBackend>(ctx, static_cast<const DpdkConfig &>(conf));
#endif
  else DPC_ERROR("unknown backend");
  return nullptr;
}

std::string Backend::getName(Backend::Kind kind) {
  for (auto &[k, n] : registry)
    if (k == kind) return n;
  DPC_FATAL("internal: unregistered backend kind");
}

Backend::Kind Backend::get(std::string_view name) {
  for (auto &[k, n] : registry)
    if (n == name) return k;
  DPC_FATAL("internal: unregistered backend name '{}'", name);
}

// std::string Backend::getName(BackendConfig const& conf) {
//   return Backend::getName(conf.kind_);
// };

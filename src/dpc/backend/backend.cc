#include "dpc/backend/backend.h"
#include "dpc/backend/dpdk/dpdk_backend.h"
#include "dpc/backend/null/null_backend.h"
#include "dpc/util/error.h"
#include <memory>

using namespace dpc;


// std::string Backend::getName(BackendConfig const& conf) {
//   return Backend::getName(conf.kind_);
// };

std::shared_ptr<Backend> Backend::create(Context &ctx, BackendConfig const &conf) {
  if (conf.is(Backend::Null))
    return std::make_shared<NullBackend>(ctx, static_cast<const NullConfig&>(conf));
  else if (conf.is(Backend::Dpdk))
#if DPC_DPDK
    return std::make_shared<DpdkBackend>(ctx, static_cast<const DpdkConfig&>(conf));
#else
    DPC_FATAL("DPDK backend not enabled (DPC_DPDK=OFF)");
#endif
  else DPC_ERROR("unknown backend");
  return nullptr;
}
#include "dpc/backend/dpdk/dpdk_backend.h"
#include "dpc/util/env.h"


namespace {
const auto kDpdkThreads = dpc::env::getuint({"DPC_WORKERS", "DPC_THREADS"});
const auto kDpdkAsync   = dpc::env::getbool({"DPC_DPDK_ASYNC"});
const auto kDpdkIface   = dpc::env::getstr({"DPC_DPDK_IFACE", "DPC_IFACE"});
} // namespace

using namespace dpc;

DpdkConfig DpdkConfig::fromJson(const std::string &path) {
  DpdkConfig c;
  return c;
}

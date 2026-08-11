#include "dpc/backend/sock/sock_backend.h"

#include "dpc/util/config.h"
#include "dpc/util/sys.h"
#include "dpc/util/env.h"
#include "dpc/util/log.h"
#include "dpc/util/net.h"
#include "dpc/util/pp.h"
#include <chrono>
#include <ctime>
#include <thread>

using namespace dpc;

SockConfig SockConfig::fromJson(const std::string &path) {
  nlohmann::json j = conf::json_load_ensure_key(path, "sock");
  SockConfig c;
  OPT(j, c, iface);
  OPT(j, c, addr);
  OPT(j, c, port);
  OPT(j, c, threads);
  OPT(j, c, window);
  OPT(j, c, timeout_us);
  OPT(j, c, tx_burst);
  OPT(j, c, tx_attempts);
  OPT(j, c, tx_interval_us);
  OPT(j, c, rx_burst);
  OPT(j, c, rx_interval_us);
  return c;
}

SockBackend::SockBackend(Context &ctx, const SockConfig &conf) : MultiworkerBackend(ctx, Backend::Sock), conf_(conf) {
  conf_.iface = env::getstr({"DPC_IFACE"}).value_or(conf_.iface);
  conf_.addr = env::getstr({"DPC_ADDR"}).value_or(conf_.addr);
  conf_.port = env::getint({"DPC_PORT"}).value_or(conf_.port);
  conf_.threads = env::getint({"DPC_WORKERS"}).value_or(conf_.threads);
  conf_.pinned = env::getbool({"DPC_PIN"}).value_or(false);
  std::tie(conf_.iface, conf_.addr) = net::resolve_endpoint(conf_.iface, conf_.addr);

  // Handle pinning
  std::vector<int> cores;
  if (conf_.pinned) {
    cores = sys::get_nic_local_cores(conf_.iface);
    if (cores.size() < conf_.threads)
      DPC_ERROR("not enough NIC-local cores for iface {}: need {} have {}", conf_.iface, conf_.threads, cores.size());
    if (cores.size() - conf_.threads <= 1)
      DPC_WARN("only {} NIC-local cores left after pinning {} workers", cores.size() - conf_.threads, conf_.threads);
  }

  // Create workers
  for (auto i = 0; i < conf_.threads; ++i)
    workers.push_back(std::make_unique<SockWorker>(*this, i, cores.size() ? cores[i]: -1));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  DPC_DEBUG("pinned cores: {}", dpc::pp::head(sys::get_pinned_cores()));
}

void SockBackend::print(bool details) const {
  DPC_INFO("backend: {}, addr={}:{} workers={}{}", name(), conf_.addr, conf_.port, conf_.threads,
           conf_.pinned ? ".pinned" : "");
}

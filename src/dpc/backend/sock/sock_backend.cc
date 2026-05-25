#include "dpc/backend/sock/sock_backend.h"

#include "dpc/util/config.h"
#include "dpc/util/env.h"

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

  // Environment variables override JSON values
  static const auto eIface = env::getstr({"DPC_IFACE"});
  static const auto eAddr = env::getstr({"DPC_ADDR"});
  static const auto ePort = env::getstr({"DPC_PORT"});

  if (eIface.has_value()) c.iface = eIface.value();
  if (eAddr.has_value()) c.addr = eAddr.value();
  if (ePort.has_value()) c.port = std::stoi(ePort.value());

  return c;
}

namespace {

const auto kAddr = env::getstr({"DPC_ADDR"});
const auto kPort = env::getstr({"DPC_PORT"});
const auto kIface = env::getstr({"DPC_IFACE"});

}; // namespace

SockBackend::SockBackend(Context &ctx, const SockConfig &conf) : MultiworkerBackend(ctx, Backend::Sock), conf_(conf) {
  if (conf_.iface.empty() and conf_.addr.empty()) {
    // conf_.iface = net::get_default_iface();
  }


  for (auto i = 0; i < conf_.threads; ++i) workers.push_back(std::make_unique<SockWorker>(1, *this, conf));
}

void SockBackend::print(bool details) const {
  DPC_INFO("backend: {}, addr={}:{} workers={}", name(), conf_.addr, conf_.port, conf_.threads);
}

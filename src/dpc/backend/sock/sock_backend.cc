#include "dpc/backend/sock/sock_backend.h"

#include "dpc/util/config.h"
#include "dpc/util/env.h"
#include "dpc/util/net.h"

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
  static const auto e_iface = env::getstr({"DPC_IFACE"});
  static const auto e_addr = env::getstr({"DPC_ADDR"});
  static const auto e_port = env::getstr({"DPC_PORT"});

  if (e_iface.has_value()) c.iface = e_iface.value();
  if (e_addr.has_value()) c.addr = e_addr.value();
  if (e_port.has_value()) c.port = std::stoi(e_port.value());

  return c;
}

namespace {

const auto kAddr = env::getstr({"DPC_ADDR"});
const auto kPort = env::getstr({"DPC_PORT"});
const auto kIface = env::getstr({"DPC_IFACE"});

}; // namespace

SockBackend::SockBackend(Context &ctx, const SockConfig &conf) : MultiworkerBackend(ctx, Backend::Sock), conf_(conf) {
  auto [iface, addr] = net::resolve_endpoint(conf.iface, conf.addr);
  conf_.iface = iface;
  conf_.addr = addr;


  for (auto i = 0; i < conf_.threads; ++i) workers.push_back(std::make_unique<SockWorker>(1, *this, conf));
}

void SockBackend::print(bool details) const {
  DPC_INFO("backend: {}, addr={}:{} workers={}", name(), conf_.addr, conf_.port, conf_.threads);
}

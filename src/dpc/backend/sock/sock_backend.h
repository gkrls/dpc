
#ifndef DPC_BACKEND_SOCK_H
#define DPC_BACKEND_SOCK_H

#include "dpc/backend/backend.h"
#include "dpc/device.h"
#include "dpc/util/config.h"

#include <netinet/in.h>
#include <sys/socket.h>

namespace dpc {

class SockConfig : public BackendConfig {
public:
  std::string iface = "eth0";
  std::string addr = "";
  uint16_t port = 4242;
  uint16_t threads = 1;
  uint16_t window = 64;
  uint64_t timeout_us = 1000;
  uint16_t tx_burst = 1;
  uint16_t tx_attempts = 5;
  uint64_t tx_interval_us = 1;
  uint16_t rx_burst = 1;
  uint64_t rx_interval_us = 1;

public:
  SockConfig() : BackendConfig(Backend::Sock){};
  static SockConfig fromJson(const std::string &path) {
    nlohmann::json root = conf::load_json(path);
    SockConfig c;
#define R(f) conf::read_if_present(c.f, root, "/sock/" #f)
    R(iface);
    R(addr);
    R(port);
    R(threads);
    R(window);
    R(timeout_us);
    R(tx_burst);
    R(tx_attempts);
    R(tx_interval_us);
    R(rx_burst);
    R(rx_interval_us);
#undef R
    return c;
  }
};

class SockWorker;

class SockBackend : public Backend {
public:
  class Net;
  SockBackend(Context &ctx, SockConfig const &conf = {});
  virtual ~SockBackend() override;
  virtual const SockConfig &config() const override { return conf; }
  virtual void print(bool details) const override;
  virtual void start() override;
  virtual void stop() override;
  virtual void push(std::shared_ptr<Task> task) override;
private:
  SockConfig conf;
};

class SockNet {
public:
  using time_point = std::chrono::steady_clock::time_point;

  SockNet() = delete;
  SockNet(uint16_t tid, SockConfig &conf, DeviceConfig &dev, size_t max_packet_size);
  ~SockNet();

  void shutdown();

  // Enqueue a packet for transmission. Auto-flushes when the queue fills.
  void tx_enqueue(void *pkt, size_t len, time_point ts);

  // Force-flush the TX queue.
  void tx_flush(time_point ts);

  // Clear queue state without sending. Used at task start.
  void tx_clear(time_point ts);

  // Receive a burst of packet
  int rx(time_point ts);

  // Receive and discard until rx is empty
  int rx_drain();

  // Retrieve the timestamp of the last time we RXed
  time_point rx_timestamp() const { return rx_ts; }

  void *rx_packet(size_t i) { return rx_iov[i].iov_base; }

private:
  const int tid = -1;
  const int max_packet_size = -1;
  const uint16_t port = 0;
  int soc = -1;
  struct sockaddr_in s_addr;
  struct sockaddr_in d_addr;

  // TX
  time_point tx_ts;
  size_t tx_size = 0;
  const size_t tx_burst;
  const size_t tx_attempts;
  const std::chrono::microseconds tx_interval;
  std::vector<mmsghdr> tx_msg;
  std::vector<iovec> tx_iov;

  // RX
  const size_t rx_burst;
  const std::chrono::microseconds rx_interval;
  std::vector<mmsghdr> rx_msg;
  std::vector<iovec> rx_iov;
  std::vector<char> rx_buf;
  time_point rx_ts;
};

class SockWorker : public BackendWorker {};

} // namespace dpc::sock

#endif // !DPC_BACKEND_SOCK_H

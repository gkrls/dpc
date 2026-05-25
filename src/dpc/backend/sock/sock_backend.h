
#ifndef DPC_BACKEND_SOCK_H
#define DPC_BACKEND_SOCK_H

#include "dpc/backend/backend.h"
#include "dpc/device.h"

#include <netinet/in.h>
#include <sys/socket.h>

namespace dpc {

class SockConfig : public BackendConfig {
public:
  std::string iface = "";
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

  SockConfig() : BackendConfig(Backend::Sock){};
  static SockConfig fromJson(const std::string &path);
};

class SockBackend;

class SockNet {
public:
  using time_point = std::chrono::steady_clock::time_point;

  SockNet() = delete;
  SockNet(uint16_t tid, const SockConfig &conf, const DeviceConfig &dev, size_t mtu = 1500);
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
  const uint16_t tid = 0;
  const uint16_t mtu = 0;
  const uint16_t port = 0;
  int soc = -1;
  struct sockaddr_in s_addr {};
  struct sockaddr_in d_addr {};

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

class SockWorker : public BackendWorker {
public:
  SockWorker(uint16_t id, SockBackend &backend, const SockConfig &conf);
  ~SockWorker() override { stop(true); }

protected:
  Task::Status execute(std::shared_ptr<Task> task) override;

private:
  SockNet net_;
  SockConfig conf_;
};

class SockBackend : public MultiworkerBackend {
public:
  SockBackend(Context &ctx, const SockConfig &conf = {});
  const SockConfig &config() const override { return conf_; }

  void print(bool details) const override;

private:
  SockConfig conf_;
};

} // namespace dpc

#endif // !DPC_BACKEND_SOCK_H

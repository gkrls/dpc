#include "dpc/backend/sock/sock_backend.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

namespace dpc {

SockNet::SockNet(uint16_t tid, SockConfig &conf, DeviceConfig &dev, size_t max_packet_size)
    : tid(tid), port(conf.port + tid), tx_burst(conf.tx_burst), tx_attempts(conf.tx_attempts),
      tx_interval(conf.tx_interval_us), rx_burst(conf.rx_burst), rx_interval(conf.rx_interval_us) {

  // TX setup. iov_len defaults to max; txEnqueue overrides per call.
  tx_msg.resize(tx_burst);
  tx_iov.resize(tx_burst);
  for (size_t i = 0; i < tx_burst; ++i) {
    tx_iov[i].iov_base = nullptr;
    tx_iov[i].iov_len = max_packet_size;
    std::memset(&tx_msg[i], 0, sizeof(mmsghdr));
    tx_msg[i].msg_hdr.msg_iov = &tx_iov[i];
    tx_msg[i].msg_hdr.msg_iovlen = 1;
    // msg_name left null; socket is connect()-ed below.
  }

  // RX setup. One owned buffer per slot, sized to max packet.
  rx_msg.resize(rx_burst);
  rx_iov.resize(rx_burst);
  rx_buf.resize(rx_burst * max_packet_size);
  for (size_t i = 0; i < rx_burst; ++i) {
    std::memset(&rx_msg[i], 0, sizeof(mmsghdr));
    rx_iov[i].iov_base = &rx_buf[i * max_packet_size];
    rx_iov[i].iov_len = max_packet_size;
    rx_msg[i].msg_hdr.msg_iov = &rx_iov[i];
    rx_msg[i].msg_hdr.msg_iovlen = 1;
  }

  soc = ::socket(AF_INET, SOCK_DGRAM, 0);
  DPC_CHECK(soc >= 0, "t{}: create socket: {}", tid, strerror(errno));

  int one = 1, zero = 0;
  setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  // Buffer sizing. Kernel doubles whatever we pass and caps at rmem_max/wmem_max.
  // Read back and warn if capped.
  int want_snd = static_cast<int>(max_packet_size * tx_burst * 4);
  int want_rcv = static_cast<int>(max_packet_size * rx_burst * 4);
  setsockopt(soc, SOL_SOCKET, SO_SNDBUF, &want_snd, sizeof(want_snd));
  setsockopt(soc, SOL_SOCKET, SO_RCVBUF, &want_rcv, sizeof(want_rcv));
  int got_snd = 0, got_rcv = 0;
  socklen_t optlen = sizeof(int);
  getsockopt(soc, SOL_SOCKET, SO_SNDBUF, &got_snd, &optlen);
  getsockopt(soc, SOL_SOCKET, SO_RCVBUF, &got_rcv, &optlen);
  if (got_snd < want_snd * 2)
    DPC_WARN("t{}: SO_SNDBUF capped at {} (wanted {}); raise net.core.wmem_max", tid, got_snd, want_snd * 2);
  if (got_rcv < want_rcv * 2)
    DPC_WARN("t{}: SO_RCVBUF capped at {} (wanted {}); raise net.core.rmem_max", tid, got_rcv, want_rcv * 2);

  DPC_CHECK(setsockopt(soc, IPPROTO_IP, IP_MTU_DISCOVER, &zero, sizeof(zero)) != -1, "t{}: disable mtu discover: {}",
            tid, strerror(errno));

  // Bind local address
  std::memset(&s_addr, 0, sizeof(s_addr));
  s_addr.sin_family = AF_INET;
  s_addr.sin_port = htons(port + tid);
  s_addr.sin_addr.s_addr = inet_addr(conf.addr.c_str());
  DPC_CHECK(bind(soc, (struct sockaddr *)&s_addr, sizeof(s_addr)) != -1, "t{}: bind {}:{}: {}", tid, conf.addr,
            ntohs(s_addr.sin_port), strerror(errno));

  // Connect to switch so sendmmsg/recvmmsg skip route lookup and address copy.
  std::memset(&d_addr, 0, sizeof(d_addr));
  d_addr.sin_family = AF_INET;
  d_addr.sin_port = htons(dev.port);
  d_addr.sin_addr.s_addr = inet_addr(dev.addr.c_str());
  DPC_CHECK(connect(soc, (struct sockaddr *)&d_addr, sizeof(d_addr)) != -1, "t{}: connect: {}", tid, strerror(errno));
  DPC_CHECK(fcntl(soc, F_SETFL, fcntl(soc, F_GETFL, 0) | O_NONBLOCK) != -1, "t{}: set non-blocking: {}", tid,
            strerror(errno));
}

SockNet::~SockNet() { shutdown(); }

void SockNet::shutdown() {
  if (soc >= 0) {
    close(soc);
    soc = -1;
  }
}

void SockNet::tx_enqueue(void *pkt, size_t len, time_point ts) {
  tx_iov[tx_size].iov_base = pkt;
  tx_iov[tx_size].iov_len = len;
  if (++tx_size == tx_burst) tx_flush(ts);
}

void SockNet::tx_flush(time_point ts) {
  size_t sent = 0, attempts = tx_attempts;
  while (sent < tx_size && attempts) {
    int n = sendmmsg(soc, &tx_msg[sent], tx_size - sent, 0);
    if (n <= 0) {
      --attempts;
      continue;
    }
    sent += n;
    attempts = tx_attempts;
  }
  // Unsent packets (if any) are dropped; retransmit path will handle them.
  tx_size = 0;
  tx_ts = ts;
}

void SockNet::tx_clear(time_point ts) {
  tx_size = 0;
  tx_ts = ts;
}

int SockNet::rx(time_point ts) {
  int n = recvmmsg(soc, &rx_msg[0], rx_burst, MSG_DONTWAIT, nullptr);
  rx_ts = ts;
  return n;
}

int SockNet::rx_drain() {
  size_t total = 0;
  while (true) {
    int n = recvmmsg(soc, &rx_msg[0], rx_burst, MSG_DONTWAIT, nullptr);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      throw std::runtime_error(std::string("recvmmsg: ") + strerror(errno));
    }
    if (n == 0) break;
    total += n;
  }
  return total;
}

} // namespace dpc

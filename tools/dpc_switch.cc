// soft_switch -- userspace stand-in for the Tofino aggregator, for testing
// allreduce algorithms locally over loopback/UDP.
//
// Usage:  ./switch config.json
//
// For now it just: loads the config, binds a UDP socket on the configured
// port, prints every packet it receives (parsed against dpc/proto.h), and
// drops you into a small CLI controller (help / dump / clear / stop).
// Aggregation/multicast comes next -- this is the scaffold.

#include "dpc/device.h"
#include "dpc/proto.h"
#include "dpc/util/bitset.h"
#include "dpc/util/config.h"

#include "dpc_switch_utils.h"
#include "fmt/core.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using clock_t_ = std::chrono::steady_clock;

using namespace dpc;
// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
struct HostInfo {
  std::string name;
  std::string mac;
  std::string ip;
};

struct Config : public DeviceConfig {
public:
  bool wire_big_endian = true;

  static Config load(const std::string &path) {
    auto j = dpc::conf::json_load(path);
    Config c;
    Config::fromJson(conf::json_load(path), c);
    if (j.contains("extra")) {
      j = j["extra"];
      OPT(j, c, wire_big_endian);
    }
    return c;
  }
};

// ---------------------------------------------------------------------------
// Packet handling
// ---------------------------------------------------------------------------
static inline uint32_t wire32(uint32_t v) {
#if DPA_WIRE_BIG_ENDIAN
  return ntohl(v);
#else
  return v;
#endif
}
static inline uint16_t wire16(uint16_t v) {
#if DPA_WIRE_BIG_ENDIAN
  return ntohs(v);
#else
  return v;
#endif
}

// static void print_packet(const Config &cfg, const sockaddr_in &src, const uint8_t *buf, ssize_t len) {
//   char ipbuf[INET_ADDRSTRLEN] = {0};
//   inet_ntop(AF_INET, &src.sin_addr, ipbuf, sizeof(ipbuf));
//   std::string ip = ipbuf;
//   auto it = cfg.ip_to_host.find(ip);
//   std::string who = (it != cfg.ip_to_host.end()) ? it->second : "?";

//   std::ostringstream os;
//   os << "[recv] " << who << " " << ip << ":" << ntohs(src.sin_port) << "  len=" << len;

//   if (len >= static_cast<ssize_t>(DPC_HEADER_SIZE)) {
//     Header h;
//     std::memcpy(&h, buf, DPA_HEADER_SIZE);
//     os << "  sess=" << wire32(h.sessid) << " seq=" << wire32(h.seqnum) << " slot=" << wire16(h.slotid)
//        << " n=" << static_cast<int>(h.n) << std::hex << std::showbase << " flags=" << static_cast<int>(h.flags)
//        << " bitmap=" << wire32(h.bitmap) << std::noshowbase << std::dec << " oper=" << wire32(h.operid)
//        << " off=" << wire32(h.offset) << " counts=" << wire16(h.counts) << " quants=" << wire32(h.quants);
//     ssize_t payload = len - DPA_HEADER_SIZE;
//     os << "  payload=" << payload << "B";
//     // first few payload bytes as hex, helps eyeball the reduction data
//     ssize_t show = payload < 16 ? payload : 16;
//     if (show > 0) {
//       os << " [";
//       for (ssize_t i = 0; i < show; ++i) {
//         char b[4];
//         std::snprintf(b, sizeof(b), "%02x", buf[DPA_HEADER_SIZE + i]);
//         os << (i ? " " : "") << b;
//       }
//       if (payload > show) os << " ...";
//       os << "]";
//     }
//   } else {
//     os << "  (runt: shorter than " << DPA_HEADER_SIZE << "B header)";
//   }
//   std::cout << os.str() << std::endl;
// }

static std::atomic<bool> g_running{true};
static std::vector<std::vector<int32_t>> g_agg;
static std::vector<std::vector<int8_t>> g_exp;
static std::vector<dpc::Bitset<64>> g_bmp;

static void loop(int sock, const Config &cfg, Stats &stats) {
  std::vector<uint8_t> buf(65536);
  while (g_running.load()) {
    sockaddr_in src{};
    socklen_t slen = sizeof(src);
    ssize_t n = recvfrom(sock, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr *>(&src), &slen);
    if (n < 0) {
      // SO_RCVTIMEO expiry -> loop back and re-check g_running
      continue;
    }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));

    stats.record_in(n, ip);

    std::cout << "[recv] " << ip << ":" << ntohs(src.sin_port) << " len=" << n << "\n";
    // print_packet(cfg, src, buf.data(), n);

    // std::lock_guard<std::mutex> lk(stats.mu);
    // stats.packets++;
    // stats.bytes += static_cast<uint64_t>(n);
    // char ipbuf[INET_ADDRSTRLEN] = {0};
    // inet_ntop(AF_INET, &src.sin_addr, ipbuf, sizeof(ipbuf));
    // auto it = cfg.ip_to_host.find(ipbuf);
    // std::string key = (it != cfg.ip_to_host.end() ? it->second : "?") + " (" + ipbuf + ")";
    // stats.by_src[key]++;
    // if (n >= static_cast<ssize_t>(DPC_HEADER_SIZE)) {
    //   Header h;
    //   std::memcpy(&h, buf.data(), DPC_HEADER_SIZE);
    //   stats.by_session[wire32(h.sessid)]++;
    // }
    std::this_thread::sleep_for(std::chrono::microseconds{20});
  }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
static int make_udp_socket(const dpc::DeviceConfig &c) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    return -1;
  }
  int one = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  // short recv timeout so the recv thread can notice g_running going false
  timeval tv{};
  tv.tv_sec = 0;
  tv.tv_usec = 200 * 1000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(c.port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY); // bind 0.0.0.0 for local testing
  if (bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    perror("bind");
    close(sock);
    return -1;
  }
  return sock;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " config.json\n";
    return 2;
  }

  Config c = Config::load(argv[1]);

  fmt::println("dpc_switch: addr={}:{} world={}-{}", c.addr, c.port, c.world_min, c.world_max);
  fmt::println("    config: pipes={} mode={} reducers={} width={} payload={}-{} slots={} sessions={}", c.pipes,
               c.reducer_mode, c.reducers, c.value_width, c.minValues(), c.maxValues(), c.reducer_slots,
               c.sessions_max);
  fmt::println("            wire_big_endian={}", c.wire_big_endian);

  int sock = make_udp_socket(c);
  if (sock < 0) return 1;
  fmt::println("dataplane listening on port {}", c.port);

  Stats stats;
  std::thread pipeline(loop, sock, std::cref(c), std::ref(stats));

  cli_controller(stats, g_running);

  g_running.store(false);
  pipeline.join();
  close(sock);
  return 0;
}

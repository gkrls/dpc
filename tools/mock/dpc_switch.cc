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
#include <csignal>
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

static std::atomic<bool> g_running{true};

static std::vector<std::vector<int32_t>> g_agg;
static std::vector<std::vector<int8_t>> g_exp;
static std::vector<dpc::Bitset<64>> g_bmp;

static void dataplane(int sock, const DeviceConfig &conf, Stats &stats, SwitchConsole::Dataplane &c) {
  c.log(fmt::format("[+] dataplane listening at {}:{}", conf.addr, conf.port));

  std::vector<uint8_t> buf(65536);
  for (auto i = 0; i < 32; i++) { c.log(fmt::format("hello {}", i)); }
  while (g_running.load()) {
    sockaddr_in src{};
    socklen_t slen = sizeof(src);
    ssize_t n = recvfrom(sock, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr *>(&src), &slen);
    if (n < 0) continue; // SO_RCVTIMEO expiry -> re-check running

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
    stats.record_in(n, ip);

    c.log(fmt::format("[recv] {}:{} len={}", ip, ntohs(src.sin_port), n));
    std::this_thread::sleep_for(std::chrono::microseconds{20});
  }
}

static void controller(Stats &stats, SwitchConsole::Controller &c) {
  while (g_running.load()) {
    if (auto r = c.read_line(g_running); r.has_value()) {
      auto cmd = r.value();
      if (cmd == "stop") {
        g_running.store(false);
      } else if (cmd == "dump") {
        // c.log(fmt::format("packets={} bytes={}", stats.packets, stats.bytes));
      } else if (cmd == "clear") {
        c.log("stats cleared");
      } else if (cmd == "help") {
        c.log("commands: help dump clear stop");
      } else if (!cmd.empty()) {
        c.log("unknown command: " + cmd);
      }
    }
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

static void on_signal(int) { g_running.store(false); }

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " config.json\n";
    return 2;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  Stats stats;
  DeviceConfig conf = DeviceConfig::fromJson(std::string(argv[1]));

  int sock = make_udp_socket(conf);
  if (sock < 0) return 1;

  SwitchConsole c;
  c.dataplane.log(fmt::format("[+] dataplane starting with config: {}", argv[1]));

  std::thread dp(dataplane, sock, std::cref(conf), std::ref(stats), std::ref(c.dataplane));

  controller(stats, c.controller);

  g_running.store(false);
  dp.join();
  close(sock);
  return 0; // ~SplitTerm runs endwin() and restores the terminal
}

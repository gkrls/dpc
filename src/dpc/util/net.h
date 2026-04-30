#ifndef DPC_UTIL_NET_H
#define DPC_UTIL_NET_H

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string>

namespace dpa {

namespace net {

static inline bool iface_exists(const std::string &ifname) { return !ifname.empty() && if_nametoindex(ifname.c_str()) != 0; }

static inline std::string iface_for_ipv4(const std::string &ip_str, bool require_up = true, bool allow_loopback = true) {
  in_addr want{};
  if (inet_pton(AF_INET, ip_str.c_str(), &want) != 1) return "";

  ifaddrs *ifaddr = nullptr;
  if (getifaddrs(&ifaddr) == -1) return "";

  std::string result;
  for (auto *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;

    unsigned flags = ifa->ifa_flags;
    if (require_up && !(flags & IFF_UP)) continue;
    if (!allow_loopback && (flags & IFF_LOOPBACK)) continue;

    auto *sin = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
    if (std::memcmp(&sin->sin_addr, &want, sizeof(in_addr)) == 0) {
      result = ifa->ifa_name; // e.g. "eth0", "en0", "lo0"
      break;
    }
  }
  freeifaddrs(ifaddr);
  return result;
}

static inline std::string ipv4_for_iface(const std::string &ifname) {
  ifaddrs *ifaddr = nullptr;
  if (getifaddrs(&ifaddr) == -1) return "";

  std::string out;
  for (auto *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
    if (std::strcmp(ifa->ifa_name, ifname.c_str()) != 0) continue;

    char buf[INET_ADDRSTRLEN] = {0};
    auto *sin = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
    if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
      out = buf;
      break;
    }
  }
  freeifaddrs(ifaddr);
  return out;
}

static inline bool ipv4_parse(const std::string& s, uint32_t& out) {
    uint32_t result = 0;
    int parts = 0;
    int i = 0;
    const int n = s.size();

    while (i < n) {
        if (parts == 4) return false;

        if (s[i] < '0' || s[i] > '9') return false;

        int value = 0;
        int digits = 0;

        while (i < n && s[i] >= '0' && s[i] <= '9') {
            value = value * 10 + (s[i] - '0');
            if (value > 255) return false;
            ++i;
            ++digits;
        }

        if (digits == 0) return false;

        result = (result << 8) | value;
        parts++;

        if (i < n) {
            if (s[i] != '.') return false;
            ++i;
            if (i == n) return false;
        }
    }

    if (parts != 4) return false;

    out = result;
    return true;
}

static inline bool ipv4_check(const std::string &ipv4) {
    // sockaddr_in sa{};
    // return inet_pton(AF_INET, ipv4.c_str(), &(sa.sin_addr)) == 1;
  uint32_t a;
  return ipv4_parse(ipv4, a);
}

static inline bool mac_parse(const std::string& s, uint64_t& out) {
    uint64_t result = 0;
    int i = 0;
    const int n = s.size();

    for (int part = 0; part < 6; ++part) {
        if (i + 1 >= n) return false;

        auto decode = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        int hi = decode(s[i++]);
        int lo = decode(s[i++]);
        if (hi < 0 || lo < 0) return false;

        result = (result << 8) | ((hi << 4) | lo);

        if (part < 5) {
            if (i >= n || s[i] != ':') return false;
            ++i;
        }
    }

    if (i != n) return false;

    out = result;
    return true;
}

static inline bool mac_check(const std::string &mac) {
  uint64_t a;
  return mac_parse(mac, a);
}

static inline bool mac_is_valid(const std::string &mac);

} // namespace net
} // namespace dpc

#endif // DPC_UTIL_NET_H
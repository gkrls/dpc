#include "dpc/util/net.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstdint>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

namespace dpc::net {

bool ipv4_parse(const std::string &s, uint32_t &out) {
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

bool ipv4_check(const std::string &ipv4) {
  // sockaddr_in sa{};
  // return inet_pton(AF_INET, ipv4.c_str(), &(sa.sin_addr)) == 1;
  uint32_t a;
  return ipv4_parse(ipv4, a);
}

bool mac_parse(const std::string &s, uint64_t &out) {
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

bool mac_check(const std::string &mac) {
  uint64_t a;
  return mac_parse(mac, a);
}

namespace {

std::string resolve_link(const std::string &path) {
  char buf[PATH_MAX] = {};
  ssize_t n = readlink(path.c_str(), buf, sizeof(buf) - 1);
  if (n < 0) return "";
  buf[n] = '\0';
  return buf;
}

std::string ipv4_of(const std::string &ifname, struct ifaddrs *ifa_list) {
  for (auto *ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
    if (ifname != ifa->ifa_name) continue;

    char buf[INET_ADDRSTRLEN] = {};
    auto *sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) return buf;
  }
  return "";
}

unsigned iface_flags(const std::string &ifname) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return 0;

  struct ifreq ifr = {};
  strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);

  unsigned flags = 0;
  if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0) flags = ifr.ifr_flags;
  close(fd);
  return flags;
}

} // anonymous namespace

std::vector<iface_info_t> list_ifaces(bool require_up, bool allow_loopback, bool allow_virtual) {
  std::vector<iface_info_t> out;

  struct ifaddrs *ifa_list = nullptr;
  if (getifaddrs(&ifa_list) != 0) return out;

  std::vector<std::string> seen;
  for (auto *ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_name) continue;
    unsigned f = ifa->ifa_flags;
    if (require_up && !(f & IFF_UP)) continue;
    if (!allow_loopback && (f & IFF_LOOPBACK)) continue;
    if (!allow_virtual && iface_is_virtual(ifa->ifa_name)) continue;

    std::string name = ifa->ifa_name;
    if (std::find(seen.begin(), seen.end(), name) != seen.end()) continue;
    seen.push_back(name);

    iface_info_t info;
    info.name = name;
    info.speed_mbps = iface_speed(name);
    info.driver = iface_driver(name);
    info.pci_addr = iface_pci_addr(name);
    info.ipv4 = ipv4_of(name, ifa_list);
    info.is_virtual = iface_is_virtual(name);
    out.push_back(std::move(info));
  }

  freeifaddrs(ifa_list);

  std::sort(out.begin(), out.end(),
            [](const iface_info_t &a, const iface_info_t &b) { return a.speed_mbps > b.speed_mbps; });

  return out;
}

std::vector<iface_info_t> list_ifaces_with_ipv4(bool require_up, bool allow_loopback, bool allow_virtual) {
  auto all = list_ifaces(require_up, allow_loopback, allow_virtual);
  all.erase(std::remove_if(all.begin(), all.end(), [](const iface_info_t &i) { return i.ipv4.empty(); }), all.end());
  return all;
}

bool iface_exists(const std::string &name) { return !name.empty() && if_nametoindex(name.c_str()) != 0; }

std::string ipv4_for_iface(const std::string &ifname) {
  struct ifaddrs *ifa_list = nullptr;
  if (getifaddrs(&ifa_list) != 0) return "";
  std::string result = ipv4_of(ifname, ifa_list);
  freeifaddrs(ifa_list);
  return result;
}

std::vector<std::string> ipv4s_for_iface(const std::string &ifname) {
  std::vector<std::string> out;
  struct ifaddrs *ifa_list = nullptr;
  if (getifaddrs(&ifa_list) != 0) return out;
  for (auto *ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
    if (ifname != ifa->ifa_name) continue;
    char buf[INET_ADDRSTRLEN] = {};
    auto *sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) out.emplace_back(buf);
  }
  freeifaddrs(ifa_list);
  return out;
}

// std::string ipv4_for_iface(const std::string &ifname) {
//   struct ifaddrs *ifa_list = nullptr;
//   if (getifaddrs(&ifa_list) != 0) return "";
//   std::string result;
//   for (auto *ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
//     if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
//     if (ifname != ifa->ifa_name) continue;
//     char buf[INET_ADDRSTRLEN] = {};
//     auto *sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
//     if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) { result = buf; break; }
//   }
//   freeifaddrs(ifa_list);
//   return result;
// }

std::string iface_for_ipv4(const std::string &ip_str, bool require_up, bool allow_loopback) {
  in_addr want{};
  if (inet_pton(AF_INET, ip_str.c_str(), &want) != 1) return "";

  struct ifaddrs *ifa_list = nullptr;
  if (getifaddrs(&ifa_list) != 0) return "";

  std::string result;
  for (auto *ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;

    unsigned flags = ifa->ifa_flags;
    if (require_up && !(flags & IFF_UP)) continue;
    if (!allow_loopback && (flags & IFF_LOOPBACK)) continue;

    auto *sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    if (std::memcmp(&sin->sin_addr, &want, sizeof(in_addr)) == 0) {
      result = ifa->ifa_name;
      break;
    }
  }
  freeifaddrs(ifa_list);
  return result;
}

bool iface_is_up(const std::string &name) { return iface_flags(name) & IFF_UP; }

bool iface_is_loopback(const std::string &name) { return iface_flags(name) & IFF_LOOPBACK; }

bool iface_is_virtual(const std::string &name) {
  static const char *prefixes[] = {
      "docker", "veth", "br-", "virbr", "vnet",
  };
  for (auto p : prefixes) {
    if (name.compare(0, strlen(p), p) == 0) return true;
  }
  return false;
}

std::string iface_driver(const std::string &name) {
  std::string link = resolve_link("/sys/class/net/" + name + "/device/driver");
  if (link.empty()) return "";
  auto pos = link.rfind('/');
  return (pos != std::string::npos) ? link.substr(pos + 1) : link;
}

std::string iface_pci_addr(const std::string &name) {
  std::string link = resolve_link("/sys/class/net/" + name + "/device");
  if (link.empty()) return "";
  auto pos = link.rfind('/');
  return (pos != std::string::npos) ? link.substr(pos + 1) : link;
}

uint32_t iface_speed(const std::string &name) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return 0;

  struct ifreq ifr = {};
  struct ethtool_cmd ecmd = {};
  ecmd.cmd = ETHTOOL_GSET;
  strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
  ifr.ifr_data = reinterpret_cast<char *>(&ecmd);

  uint32_t speed = 0;
  if (ioctl(fd, SIOCETHTOOL, &ifr) == 0) speed = ethtool_cmd_speed(&ecmd);
  close(fd);
  return (speed == (uint32_t)-1) ? 0 : speed;
}

void print_ifaces(bool require_up, bool allow_loopback, bool allow_virtual) {
  auto ifaces = list_ifaces(require_up, allow_loopback, allow_virtual);
  if (ifaces.empty()) {
    printf("  (no interfaces found)\n");
    return;
  }
  printf("  %-16s %-16s %10s  %-14s %s\n", "NAME", "IPV4", "SPEED", "DRIVER", "PCI");
  printf("  %-16s %-16s %10s  %-14s %s\n", "----", "----", "-----", "------", "---");
  for (auto &i : ifaces) {
    printf("  %-16s %-16s %7u Mb  %-14s %s\n", i.name.c_str(), i.ipv4.empty() ? "-" : i.ipv4.c_str(), i.speed_mbps,
           i.driver.empty() ? "-" : i.driver.c_str(), i.pci_addr.empty() ? "-" : i.pci_addr.c_str());
  }
}

// Resolves (iface, addr) from possibly-empty inputs. Throws on any
// ambiguous, invalid, or unsatisfiable configuration.
//
// Priority: user input is always respected when given. We only ever fill in
// values the user left empty; we never override what the user provided.
//
// Cases (by which of {iface, addr} the user provided):
//
//   1. BOTH given
//      Validate that `addr` is actually bound to `iface`. Reject otherwise.
//
//   2. IFACE only
//      Look up all IPv4 addresses on the iface.
//        0 addrs  -> error (nothing to bind to; iface may be down or unconfigured)
//        1 addr   -> use it
//        N addrs  -> error, list them and require disambiguation.
//      Rationale: multiple IPs on one iface usually means aliases, VIPs, or
//      multi-subnet setups. Kernel enumeration order is not meaningful, so
//      picking "the first" is a coin flip with real failure modes (binding
//      to a passive VIP, wrong subnet, etc.).
//
//   3. ADDR only
//      Find the iface that owns the addr. Loopback is allowed here because
//      the user was explicit.
//        0 ifaces -> error
//
//   4. NEITHER given (auto-select)
//      Enumerate candidates via list_ifaces_with_ipv4(up=true, loopback=false,
//      virtual=false) — i.e. real, up, IP-bearing physical interfaces.
//        0 candidates -> error
//        1 candidate  -> use it
//        N candidates -> pick UNIQUE fastest by link speed (list_ifaces sorts
//                        desc by speed). If the top speed is tied across
//                        multiple ifaces, error and list candidates.
//      Rationale: on the target deployment (HPC/datacenter), the data NIC is
//      typically 10-100x faster than the mgmt NIC (e.g. 100 GbE vs 1 GbE),
//      so "unique max speed" is both safe and almost always decisive. When
//      it isn't decisive (genuinely symmetric multi-NIC hosts), guessing is
//      dangerous, so we refuse and let the operator pick via env/config.
std::pair<std::string, std::string> resolve_endpoint(const std::string &want_iface, const std::string &want_addr) {
  // --- Case 1: both given ---------------------------------------------------
  if (!want_iface.empty() && !want_addr.empty()) {
    if (!iface_exists(want_iface)) throw std::runtime_error("iface '" + want_iface + "' does not exist");
    if (!ipv4_check(want_addr)) throw std::runtime_error("invalid IPv4 address: " + want_addr);

    auto ips = ipv4s_for_iface(want_iface);
    if (std::find(ips.begin(), ips.end(), want_addr) == ips.end()) {
      std::string have;
      for (size_t i = 0; i < ips.size(); ++i) {
        if (i) have += ", ";
        have += ips[i];
      }
      throw std::runtime_error("addr " + want_addr + " is not on iface " + want_iface +
                               " (iface has: " + (have.empty() ? "none" : have) + ")");
    }
    return {want_iface, want_addr};
  }

  // --- Case 2: iface only ---------------------------------------------------
  if (!want_iface.empty()) {
    if (!iface_exists(want_iface)) throw std::runtime_error("iface '" + want_iface + "' does not exist");

    auto ips = ipv4s_for_iface(want_iface);
    if (ips.empty()) throw std::runtime_error("iface '" + want_iface + "' has no IPv4 address");
    if (ips.size() > 1) {
      std::string list;
      for (size_t i = 0; i < ips.size(); ++i) {
        if (i) list += ", ";
        list += ips[i];
      }
      throw std::runtime_error("iface '" + want_iface + "' has multiple IPv4 addresses (" + list +
                               "); specify one explicitly");
    }
    return {want_iface, ips[0]};
  }

  // --- Case 3: addr only ----------------------------------------------------
  if (!want_addr.empty()) {
    if (!ipv4_check(want_addr)) throw std::runtime_error("invalid IPv4 address: " + want_addr);
    // Loopback allowed: user was explicit.
    std::string name = iface_for_ipv4(want_addr, /*require_up=*/true, /*allow_loopback=*/true);
    if (name.empty()) throw std::runtime_error("no interface owns address " + want_addr);
    return {name, want_addr};
  }

  // --- Case 4: neither given — auto-select ---------------------------------
  auto candidates = list_ifaces_with_ipv4(/*require_up=*/true, /*allow_loopback=*/false, /*allow_virtual=*/false);
  if (candidates.empty())
    throw std::runtime_error("no usable network interface found; "
                             "specify iface or addr explicitly");
  if (candidates.size() == 1) return {candidates[0].name, candidates[0].ipv4};

  // list_ifaces sorts desc by speed_mbps. Top is unique iff exactly one
  // candidate has speed == candidates[0].speed_mbps.
  uint32_t top = candidates[0].speed_mbps;
  int tied = 0;
  for (auto &c : candidates)
    if (c.speed_mbps == top) ++tied;
  if (tied == 1) return {candidates[0].name, candidates[0].ipv4};

  std::string list;
  for (auto &c : candidates) { list += "\n  " + c.name + " " + c.ipv4 + " " + std::to_string(c.speed_mbps) + " Mb/s"; }
  throw std::runtime_error("multiple interfaces tied at top speed (" + std::to_string(top) +
                           " Mb/s); specify iface or addr explicitly. "
                           "candidates:" +
                           list);
}

} // namespace dpc::net

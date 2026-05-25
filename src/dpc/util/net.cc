#include "dpc/util/net.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstdint>
#include <linux/ethtool.h>
#include <linux/sockios.h>
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

} // namespace dpc::net

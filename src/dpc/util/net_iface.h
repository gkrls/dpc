#ifndef DPC_UTIL_NET_IFACE_H
#define DPC_UTIL_NET_IFACE_H

#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <string>
#include <vector>

namespace dpc::net {

/**
 * @brief Retrieve the first interface found with @p addr bound to it (if any)
 * @param addr ipv4 address to query
 * @param require_up if true, only search interfaces that are up
 * @param allow_loopback if true, include loopback interfaces
 * @return the interface name, if one is found, otherwise the empty string
 */
std::string iface_for_ipv4(const std::string &addr, bool require_up = true, bool allow_loopback = false);

/**
 * @brief Retrieve the first ipv4 address bound to interface @p ifname
 * @param ifname interface name to query
 * @return an ipv4 address string. The empty string, If none found or ifname does not exist
 */
std::string ipv4_for_iface(const std::string &ifname);

struct iface_info_t {
  std::string name;
  std::string ipv4;     // empty if none
  uint32_t speed_mbps;  // 0 if unknown
  std::string driver;   // kernel driver, e.g. "mlx5_core"
  std::string pci_addr; // e.g. "0000:41:00.0", empty if virtual
  bool is_virtual;
};

/// Check if an interface with name @p ifname exists
bool iface_exists(const std::string &ifname);
bool iface_is_up(const std::string &name);
bool iface_is_virtual(const std::string &name);
bool iface_is_loopback(const std::string &name);
std::string iface_driver(const std::string &name);
std::string iface_pci_addr(const std::string &name);
uint32_t iface_speed(const std::string &name);

/// All UP, non-loopback, non-virtual (docker/veth/br-/virbr) interfaces,
/// sorted by speed descending.
std::vector<iface_info_t> list_ifaces(bool require_up = true, bool allow_loopback = false, bool allow_virtual = false);

/// Retrieve all interfaces with an an IPv4 address.
std::vector<iface_info_t> list_ifaces_with_ipv4(bool require_up = true, bool allow_loopback = false,
                                                bool allow_virtual = false);

void print_ifaces(bool require_up = true, bool allow_loopback = false, bool allow_virtual = false);

} // namespace dpc::net

#endif // !DPC_UTIL_NET_IFACE_H
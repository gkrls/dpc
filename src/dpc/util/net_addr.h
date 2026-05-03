#ifndef DPC_UTIL_NET_ADDR_H
#define DPC_UTIL_NET_ADDR_H

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string>

namespace dpc {

namespace net {

/// Parse an ipv4 address string @p s into a 32-bit int @p out
/// On success true is returned and @p out is written. Otherwise false
bool ipv4_parse(const std::string& s, uint32_t& out);

/// Check if ipv4 address string @p s is a valid ipv4 address
bool ipv4_check(const std::string &s);

/// Parse a mac address string @p s into a 64-bit int @p out
/// On success true is returned and @p out is written. Otherwise false
bool mac_parse(const std::string& s, uint64_t& out);

/// Check if mac address string @p s is a valid mac address
bool mac_check(const std::string &mac);


} // namespace net
} // namespace dpc

#endif // !DPC_UTIL_NET_ADDR_H
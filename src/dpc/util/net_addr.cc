#include "dpc/util/net_addr.h"

namespace dpc::net {

bool ipv4_parse(const std::string& s, uint32_t& out) {
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

bool mac_parse(const std::string& s, uint64_t& out) {
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

} // namespace dpc::net
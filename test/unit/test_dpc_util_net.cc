#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "dpc/util/net.h"

#include "doctest/doctest.h"

using namespace dpc::net;

// ---------------------------------------------------------------------------
// ipv4_parse
// ---------------------------------------------------------------------------

TEST_CASE("ipv4_parse: basic valid") {
  uint32_t out = 0;
  CHECK(ipv4_parse("192.168.1.1", out));
  CHECK(out == 0xC0A80101);
}

TEST_CASE("ipv4_parse: all zeros") {
  uint32_t out = 0;
  CHECK(ipv4_parse("0.0.0.0", out));
  CHECK(out == 0x00000000);
}

TEST_CASE("ipv4_parse: all 255") {
  uint32_t out = 0;
  CHECK(ipv4_parse("255.255.255.255", out));
  CHECK(out == 0xFFFFFFFF);
}

TEST_CASE("ipv4_parse: 10.0.0.1") {
  uint32_t out = 0;
  CHECK(ipv4_parse("10.0.0.1", out));
  CHECK(out == 0x0A000001);
}

TEST_CASE("ipv4_parse: octet 256 rejected") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("256.0.0.1", out));
}

TEST_CASE("ipv4_parse: octet 999 rejected") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("1.2.999.4", out));
}

TEST_CASE("ipv4_parse: too few octets") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("1.2.3", out));
}

TEST_CASE("ipv4_parse: too many octets") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("1.2.3.4.5", out));
}

TEST_CASE("ipv4_parse: empty string") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("", out));
}

TEST_CASE("ipv4_parse: trailing dot") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("1.2.3.4.", out));
}

TEST_CASE("ipv4_parse: leading dot") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse(".1.2.3.4", out));
}

TEST_CASE("ipv4_parse: double dot") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("1..2.3", out));
}

TEST_CASE("ipv4_parse: letters rejected") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("1.2.a.4", out));
}

TEST_CASE("ipv4_parse: spaces rejected") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("1. 2.3.4", out));
}

TEST_CASE("ipv4_parse: negative rejected") {
  uint32_t out = 0;
  CHECK_FALSE(ipv4_parse("-1.2.3.4", out));
}

TEST_CASE("ipv4_parse: does not clobber out on failure") {
  uint32_t out = 0xDEADBEEF;
  CHECK_FALSE(ipv4_parse("bad", out));
  CHECK(out == 0xDEADBEEF);
}

// ---------------------------------------------------------------------------
// ipv4_check
// ---------------------------------------------------------------------------

TEST_CASE("ipv4_check: valid returns true") {
  CHECK(ipv4_check("10.0.0.1"));
  CHECK(ipv4_check("0.0.0.0"));
  CHECK(ipv4_check("255.255.255.255"));
}

TEST_CASE("ipv4_check: invalid returns false") {
  CHECK_FALSE(ipv4_check(""));
  CHECK_FALSE(ipv4_check("999.0.0.1"));
  CHECK_FALSE(ipv4_check("hello"));
}

// ---------------------------------------------------------------------------
// mac_parse
// ---------------------------------------------------------------------------

TEST_CASE("mac_parse: basic valid lowercase") {
  uint64_t out = 0;
  CHECK(mac_parse("aa:bb:cc:dd:ee:ff", out));
  CHECK(out == 0xAABBCCDDEEFF);
}

TEST_CASE("mac_parse: basic valid uppercase") {
  uint64_t out = 0;
  CHECK(mac_parse("AA:BB:CC:DD:EE:FF", out));
  CHECK(out == 0xAABBCCDDEEFF);
}

TEST_CASE("mac_parse: mixed case") {
  uint64_t out = 0;
  CHECK(mac_parse("aA:Bb:cC:00:11:22", out));
  CHECK(out == 0xAABBCC001122);
}

TEST_CASE("mac_parse: all zeros") {
  uint64_t out = 0;
  CHECK(mac_parse("00:00:00:00:00:00", out));
  CHECK(out == 0x000000000000);
}

TEST_CASE("mac_parse: all ff") {
  uint64_t out = 0;
  CHECK(mac_parse("ff:ff:ff:ff:ff:ff", out));
  CHECK(out == 0xFFFFFFFFFFFF);
}

TEST_CASE("mac_parse: specific address") {
  uint64_t out = 0;
  CHECK(mac_parse("02:42:ac:11:00:02", out));
  CHECK(out == 0x0242AC110002);
}

TEST_CASE("mac_parse: too few groups") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("aa:bb:cc:dd:ee", out));
}

TEST_CASE("mac_parse: too many groups") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("aa:bb:cc:dd:ee:ff:00", out));
}

TEST_CASE("mac_parse: empty string") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("", out));
}

TEST_CASE("mac_parse: single hex digit per group rejected") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("a:b:c:d:e:f", out));
}

TEST_CASE("mac_parse: three hex digits per group rejected") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("aaa:bb:cc:dd:ee:ff", out));
}

TEST_CASE("mac_parse: dash separator rejected") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("aa-bb-cc-dd-ee-ff", out));
}

TEST_CASE("mac_parse: invalid hex char rejected") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("gg:bb:cc:dd:ee:ff", out));
}

TEST_CASE("mac_parse: trailing colon rejected") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("aa:bb:cc:dd:ee:ff:", out));
}

TEST_CASE("mac_parse: space in input rejected") {
  uint64_t out = 0;
  CHECK_FALSE(mac_parse("aa:bb: c:dd:ee:ff", out));
}

TEST_CASE("mac_parse: does not clobber out on failure") {
  uint64_t out = 0xDEADDEADDEAD;
  CHECK_FALSE(mac_parse("nope", out));
  CHECK(out == 0xDEADDEADDEAD);
}

// ---------------------------------------------------------------------------
// mac_check
// ---------------------------------------------------------------------------

TEST_CASE("mac_check: valid returns true") {
  CHECK(mac_check("aa:bb:cc:dd:ee:ff"));
  CHECK(mac_check("00:00:00:00:00:00"));
}

TEST_CASE("mac_check: invalid returns false") {
  CHECK_FALSE(mac_check(""));
  CHECK_FALSE(mac_check("not-a-mac"));
  CHECK_FALSE(mac_check("aa:bb:cc:dd:ee"));
}

// ---------------------------------------------------------------------------
// resolve_endpoint
// ---------------------------------------------------------------------------

TEST_CASE("resolve_endpoint: case 1 — both given, addr on iface") {
  auto [iface, addr] = resolve_endpoint("lo", "127.0.0.1");
  CHECK(iface == "lo");
  CHECK(addr == "127.0.0.1");
}

TEST_CASE("resolve_endpoint: case 1 — addr not on iface throws") {
  CHECK_THROWS_AS(resolve_endpoint("lo", "8.8.8.8"), std::runtime_error);
}

TEST_CASE("resolve_endpoint: case 1 — invalid addr throws") {
  CHECK_THROWS_AS(resolve_endpoint("lo", "not_an_ip"), std::runtime_error);
}

TEST_CASE("resolve_endpoint: case 1/2 — nonexistent iface throws") {
  CHECK_THROWS_AS(resolve_endpoint("definitely_not_a_real_iface_xyz", ""), std::runtime_error);
  CHECK_THROWS_AS(resolve_endpoint("definitely_not_a_real_iface_xyz", "127.0.0.1"), std::runtime_error);
}

TEST_CASE("resolve_endpoint: case 2 — iface with single addr resolves") {
  // lo normally has exactly one IPv4 (127.0.0.1). Skip if the test box is
  // unusual to avoid false negatives.
  auto ips = ipv4s_for_iface("lo");
  if (ips.size() == 1) {
    auto [iface, addr] = resolve_endpoint("lo", "");
    CHECK(iface == "lo");
    CHECK(addr == "127.0.0.1");
  }
}

TEST_CASE("resolve_endpoint: case 3 — addr only resolves to owning iface") {
  auto [iface, addr] = resolve_endpoint("", "127.0.0.1");
  CHECK(iface == "lo");
  CHECK(addr == "127.0.0.1");
}

TEST_CASE("resolve_endpoint: case 3 — invalid addr throws") {
  CHECK_THROWS_AS(resolve_endpoint("", "not_an_ip"), std::runtime_error);
}

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "dpc/util/bitset.h"

using dpc::Bitset;

TEST_CASE("set: individual bits, 64") {
    Bitset<64> b;
    CHECK(b.empty());
    b.set(0);
    CHECK(b.check(0));
    CHECK_FALSE(b.check(1));
    b.set(63);
    CHECK(b.check(63));
    CHECK(b.count() == 2);
}

TEST_CASE("set: individual bits, 128") {
    Bitset<128> b;
    b.set(0);
    b.set(64);
    b.set(127);
    CHECK(b.check(0));
    CHECK(b.check(64));
    CHECK(b.check(127));
    CHECK_FALSE(b.check(1));
    CHECK_FALSE(b.check(63));
    CHECK(b.count() == 3);
}

TEST_CASE("clear: individual bit") {
    Bitset<64> b;
    b.set(10);
    b.set(20);
    CHECK(b.count() == 2);
    b.clear(10);
    CHECK_FALSE(b.check(10));
    CHECK(b.check(20));
    CHECK(b.count() == 1);
}

TEST_CASE("ones: count, 64") {
    Bitset<64> b;
    b.ones();
    CHECK(b.count() == 64);
    CHECK_FALSE(b.empty());
}

TEST_CASE("ones: count, 128") {
    Bitset<128> b;
    b.ones();
    CHECK(b.count() == 128);
}

TEST_CASE("zeros: clears everything") {
    Bitset<128> b;
    b.ones();
    b.zeros();
    CHECK(b.empty());
    CHECK(b.count() == 0);
}

TEST_CASE("clear: same as zeros") {
    Bitset<64> b;
    b.ones();
    b.clear();
    CHECK(b.empty());
}

TEST_CASE("rset: rightmost n bits, 64") {
    Bitset<64> b;
    b.rset(4);
    CHECK(b.count() == 4);
    CHECK(b.check(0));
    CHECK(b.check(3));
    CHECK_FALSE(b.check(4));
}

TEST_CASE("rset: all bits, 64") {
    Bitset<64> b;
    b.rset(64);
    CHECK(b.count() == 64);
}

TEST_CASE("rset: n > N saturates, 64") {
    Bitset<64> b;
    b.rset(100);
    CHECK(b.count() == 64);
}

TEST_CASE("rset: multi-word, 128") {
    Bitset<128> b;
    b.rset(70);
    CHECK(b.count() == 70);
    CHECK(b.check(0));
    CHECK(b.check(69));
    CHECK_FALSE(b.check(70));
}

TEST_CASE("rset: n=0 does nothing") {
    Bitset<64> b;
    b.rset(0);
    CHECK(b.empty());
}

TEST_CASE("lset: leftmost n bits, 64") {
    Bitset<64> b;
    b.lset(4);
    CHECK(b.count() == 4);
    CHECK(b.check(63));
    CHECK(b.check(60));
    CHECK_FALSE(b.check(59));
}

TEST_CASE("lset: all bits, 64") {
    Bitset<64> b;
    b.lset(64);
    CHECK(b.count() == 64);
}

TEST_CASE("lset: multi-word, 128") {
    Bitset<128> b;
    b.lset(70);
    CHECK(b.count() == 70);
    CHECK(b.check(127));
    CHECK(b.check(58));
    CHECK_FALSE(b.check(57));
}

TEST_CASE("rclear: rightmost n bits") {
    Bitset<64> b;
    b.ones();
    b.rclear(10);
    CHECK(b.count() == 54);
    CHECK_FALSE(b.check(0));
    CHECK_FALSE(b.check(9));
    CHECK(b.check(10));
}

TEST_CASE("rclear: all bits, 64") {
    Bitset<64> b;
    b.ones();
    b.rclear(64);
    CHECK(b.empty());
}

TEST_CASE("rclear: n > N saturates, 64") {
    Bitset<64> b;
    b.ones();
    b.rclear(100);
    CHECK(b.empty());
}

TEST_CASE("rclear: multi-word, 128") {
    Bitset<128> b;
    b.ones();
    b.rclear(70);
    CHECK(b.count() == 58);
    CHECK_FALSE(b.check(69));
    CHECK(b.check(70));
}

TEST_CASE("lclear: leftmost n bits") {
    Bitset<64> b;
    b.ones();
    b.lclear(10);
    CHECK(b.count() == 54);
    CHECK_FALSE(b.check(63));
    CHECK_FALSE(b.check(54));
    CHECK(b.check(53));
}

TEST_CASE("lclear: all bits, 64") {
    Bitset<64> b;
    b.ones();
    b.lclear(64);
    CHECK(b.empty());
}

TEST_CASE("lclear: multi-word, 128") {
    Bitset<128> b;
    b.ones();
    b.lclear(70);
    CHECK(b.count() == 58);
    CHECK_FALSE(b.check(127));
    CHECK_FALSE(b.check(58));
    CHECK(b.check(57));
}

TEST_CASE("bitstring: empty, 64") {
    Bitset<64> b;
    std::string s = b.bitstring();
    CHECK(s.size() == 64);
    CHECK(s == std::string(64, '0'));
}

TEST_CASE("bitstring: bit 0 set, 64") {
    Bitset<64> b;
    b.set(0);
    std::string s = b.bitstring();
    CHECK(s.back() == '1');
    CHECK(s.front() == '0');
}

TEST_CASE("bitstring: MSB set, 64") {
    Bitset<64> b;
    b.set(63);
    std::string s = b.bitstring();
    CHECK(s.front() == '1');
    CHECK(s.back() == '0');
}

TEST_CASE("bitstring: 128-bit, low and high") {
    Bitset<128> b;
    b.set(0);
    b.set(127);
    std::string s = b.bitstring();
    CHECK(s.size() == 128);
    CHECK(s.front() == '1');
    CHECK(s.back() == '1');
    CHECK(s[1] == '0');
    CHECK(s[126] == '0');
}

TEST_CASE("bitstring: rset matches pattern") {
    Bitset<64> b;
    b.rset(4);
    std::string s = b.bitstring();
    CHECK(s == std::string(60, '0') + "1111");
}

TEST_CASE("combined: rset then lclear") {
    Bitset<128> b;
    b.rset(128);
    b.lclear(64);
    CHECK(b.count() == 64);
    CHECK(b.check(0));
    CHECK(b.check(63));
    CHECK_FALSE(b.check(64));
}

TEST_CASE("combined: lset then rclear") {
    Bitset<128> b;
    b.lset(128);
    b.rclear(64);
    CHECK(b.count() == 64);
    CHECK_FALSE(b.check(0));
    CHECK(b.check(64));
    CHECK(b.check(127));
}
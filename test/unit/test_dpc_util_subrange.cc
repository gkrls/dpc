#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "dpc/util/subrange.h"


using dpc::Subrange;

TEST_CASE("partition: unit_size=1, basic element partitioning") {
  auto r0 = Subrange::partition(0, 3, 0, 10, 1);
  CHECK(r0.lo == 0);
  CHECK(r0.hi == 4);
  CHECK(r0.units == 4);

  auto r1 = Subrange::partition(1, 3, 0, 10, 1);
  CHECK(r1.lo == 4);
  CHECK(r1.hi == 7);
  CHECK(r1.units == 3);

  auto r2 = Subrange::partition(2, 3, 0, 10, 1);
  CHECK(r2.lo == 7);
  CHECK(r2.hi == 10);
  CHECK(r2.units == 3);
}

TEST_CASE("partition: unit_size=3, remainder unit is short") {
  auto r0 = Subrange::partition(0, 3, 0, 100, 3);
  CHECK(r0.lo == 0);
  CHECK(r0.hi == 36);
  CHECK(r0.units == 12);

  auto r1 = Subrange::partition(1, 3, 0, 100, 3);
  CHECK(r1.lo == 36);
  CHECK(r1.hi == 69);
  CHECK(r1.units == 11);

  auto r2 = Subrange::partition(2, 3, 0, 100, 3);
  CHECK(r2.lo == 69);
  CHECK(r2.hi == 100);
  CHECK(r2.units == 11);
}

TEST_CASE("partition: evenly divisible, no remainders at all") {
  auto r0 = Subrange::partition(0, 3, 0, 12, 4);
  CHECK(r0.lo == 0);
  CHECK(r0.hi == 4);
  CHECK(r0.units == 1);

  auto r1 = Subrange::partition(1, 3, 0, 12, 4);
  CHECK(r1.lo == 4);
  CHECK(r1.hi == 8);
  CHECK(r1.units == 1);

  auto r2 = Subrange::partition(2, 3, 0, 12, 4);
  CHECK(r2.lo == 8);
  CHECK(r2.hi == 12);
  CHECK(r2.units == 1);
}

TEST_CASE("partition: single part gets everything") {
  auto r = Subrange::partition(0, 1, 0, 100, 7);
  CHECK(r.lo == 0);
  CHECK(r.hi == 100);
  CHECK(r.units == 15); // ceil(100/7)
}

TEST_CASE("partition: non-zero lo offset") {
  auto r0 = Subrange::partition(0, 2, 10, 20, 3);
  auto r1 = Subrange::partition(1, 2, 10, 20, 3);
  CHECK(r0.lo == 10);
  CHECK(r0.hi == r1.lo); // contiguous
  CHECK(r1.hi == 20);
}

TEST_CASE("partition: out of range part returns empty") {
  auto r = Subrange::partition(5, 3, 0, 10, 1);
  CHECK(r.lo == 10);
  CHECK(r.hi == 10);
  CHECK(r.units == 0);
  CHECK(r.unit_size == 0);
}

TEST_CASE("partition: empty range returns empty") {
  auto r = Subrange::partition(0, 3, 5, 5, 1);
  CHECK(r.lo == 5);
  CHECK(r.hi == 5);
  CHECK(r.units == 0);
}

TEST_CASE("partition: more parts than units, excess parts are empty") {
  // 3 elements, unit_size=1, 5 parts -> only 3 parts get work
  for (uint32_t i = 3; i < 5; ++i) {
    auto r = Subrange::partition(i, 5, 0, 3, 1);
    CHECK(r.lo == r.hi);
    CHECK(r.units == 0);
  }
}

TEST_CASE("partition: all parts are contiguous and cover full range") {
  uint32_t lo = 7, hi = 93, n_parts = 5, unit_size = 4;
  uint32_t prev = lo;
  for (uint32_t i = 0; i < n_parts; ++i) {
    auto r = Subrange::partition(i, n_parts, lo, hi, unit_size);
    CHECK(r.lo == prev);
    CHECK(r.hi >= r.lo);
    prev = r.hi;
  }
  CHECK(prev == hi);
}

TEST_CASE("partition: large range, no overflow") {
  uint32_t hi = UINT32_MAX;
  auto r = Subrange::partition(0, 2, 0, hi, 1024);
  CHECK(r.lo == 0);
  CHECK(r.hi > 0);
  CHECK(r.hi <= hi);
}

TEST_CASE("partition: convenience overload assumes lo=0") {
  auto a = Subrange::partition(1, 3, 0, 50, 4);
  auto b = Subrange::partition(1, 3, 50, 4);
  CHECK(a.lo == b.lo);
  CHECK(a.hi == b.hi);
  CHECK(a.units == b.units);
}

TEST_CASE("offsets: matches partition lo values") {
  uint32_t lo = 0, hi = 100, n_parts = 4, unit_size = 3;
  auto offs = Subrange::offsets(n_parts, lo, hi, unit_size);
  CHECK(offs.size() == n_parts);
  for (uint32_t i = 0; i < n_parts; ++i) { CHECK(offs[i] == Subrange::partition(i, n_parts, lo, hi, unit_size).lo); }
}
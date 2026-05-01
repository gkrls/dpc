#ifndef DPC_UTIL_SUBRANGE_H
#define DPC_UTIL_SUBRANGE_H

#include <cstdint>
#include <vector>

namespace dpc {

struct Subrange {
  uint32_t lo;
  uint32_t hi;
  uint32_t units;     // number of whole units in [lo, hi)
  uint32_t unit_size; // size of each unit

  /// Partition [lo, hi) into @p n_parts slices at @p unit_size granularity.
  /// Return the subrange for part @p part (0-indexed). Empty if out of range.
  ///
  /// The range is first divided into ceil((hi-lo) / unit_size) units.
  /// These units are distributed as evenly as possible across parts;
  /// the first (n_units % n_parts) parts each receive one extra unit.
  ///
  /// The last unit may be shorter than unit_size if (hi-lo) is not
  /// a multiple of unit_size.
  ///
  /// Examples (unit_size=1, pure element-level partitioning):
  ///   partition(0, 3, 0, 10, 1) -> {0,  4,  4, 1}
  ///   partition(1, 3, 0, 10, 1) -> {4,  7,  3, 1}
  ///   partition(2, 3, 0, 10, 1) -> {7, 10,  3, 1}
  ///
  /// Examples (unit_size=3, 34 units, remainder unit has 1 element):
  ///   partition(0, 3, 0, 100, 3) -> {0,  36, 12, 3}   (36 elements)
  ///   partition(1, 3, 0, 100, 3) -> {36, 69, 11, 3}   (33 elements)
  ///   partition(2, 3, 0, 100, 3) -> {69, 100, 11, 3}  (31 elements, last unit short)
  static inline Subrange partition(uint32_t part, uint32_t n_parts, uint32_t lo, uint32_t hi, uint32_t unit_size) {
    if (part >= n_parts || lo >= hi) return {hi, hi, 0, 0};

    uint32_t size = hi - lo;
    // uint32_t n_units = (size + unit_size - 1) / unit_size;
    uint32_t n_units = (uint32_t)(((uint64_t)size + unit_size - 1) / unit_size);
    uint32_t per = n_units / n_parts;
    uint32_t rem = n_units % n_parts;

    uint32_t units = per + (part < rem);
    uint32_t start_u = part * per + std::min(part, rem);

    uint64_t start = lo + (uint64_t)start_u * unit_size;
    uint64_t end = lo + (uint64_t)(start_u + units) * unit_size;

    return {(uint32_t)std::min<uint64_t>(start, hi), (uint32_t)std::min<uint64_t>(end, hi), units, unit_size};
  }

  static inline Subrange partition(uint32_t part, uint32_t n_parts, uint32_t n, uint32_t unit_size) {
    return partition(part, n_parts, 0, n, unit_size);
  }

  /// Return the lo-offset of every part (useful for scatter/gather offset tables).
  static inline std::vector<uint32_t> offsets(uint32_t n_parts, uint32_t lo, uint32_t hi, uint32_t unit_size) {
    std::vector<uint32_t> res;
    res.reserve(n_parts);
    for (uint32_t i = 0; i < n_parts; ++i) res.push_back(partition(i, n_parts, lo, hi, unit_size).lo);
    return res;
  }

  static inline std::vector<uint32_t> offsets(uint32_t n_parts, uint32_t size, uint32_t unit_size) {
    return offsets(n_parts, 0, size, unit_size);
  }
};

} // namespace dpc

#endif // !DPC_UTIL_SUBRANGE_H
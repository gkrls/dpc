
#ifndef DPC_UTIL_SERDES_H
#define DPC_UTIL_SERDES_H

#include <cstddef>
#include <cstdint>

namespace dpc {
namespace serdes {

/// Convert a vector from host byte order to network byte order.
/// Returns the number of bytes converted.
size_t htonlv(void *dst, const uint32_t *src, size_t count);

/// Convert a vector from network byte order to host byte order.
/// Returns the number of bytes converted.
size_t ntohlv(uint32_t *dst, const void *src, size_t count);

/// Convert a vector from network byte order to host byte order and scale it by a float @p scale.
/// Returns the number of bytes written to dst.
size_t ntohlv_scale_to_float(float *dst, const void *src, size_t count, float scale, bool input_signed);

/// Convert a vector from network byte order to host byte order and scale it (integer version).
/// Returns the number of bytes converted.
size_t ntohlv_scale(uint32_t *dst, const void *src, size_t count, uint32_t scale);

/// Network-to-host + average, in float.
inline size_t ntohlv_avg(float *dst, const void *src, size_t count, float avg_amount, bool input_signed = true) {
  return ntohlv_scale_to_float(dst, src, count, 1.0f / avg_amount, input_signed);
}

} // namespace serdes
} // namespace dpa

#endif // !DPC_UTIL_SERDES_H
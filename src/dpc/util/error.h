#ifndef DPC_UTIL_ERROR_H
#define DPC_UTIL_ERROR_H

#include "dpc/util/log.h" // IWYU pragma: keep

#include <stdexcept> // IWYU pragma: keep

#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

#ifndef NDEBUG
#define DPC_ASSERT(pred, fstr, ...)                                                                                    \
  do {                                                                                                                 \
    if (!(pred)) {                                                                                                     \
      fmt::println(stderr, "dpc.assert {}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);             \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (0)
#else
#define DPC_ASSERT(pred, fstr, ...)                                                                                    \
  do { (void)sizeof(pred); } while (0)
#endif

#define DPC_ERROR(fstr, ...)                                                                                           \
  do {                                                                                                                 \
    auto msg = fmt::format("{}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                        \
    throw std::runtime_error(msg);                                                                                     \
  } while (0)

#define DPC_FATAL(fstr, ...)                                                                                           \
  do {                                                                                                                 \
    fmt::println(stderr, "dpc.fatal {}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                \
    std::abort();                                                                                                      \
  } while (0)

#define DPC_ERROR_IF(cond, fstr, ...)                                                                                  \
  do {                                                                                                                 \
    if (cond) DPC_ERROR(fstr __VA_OPT__(, ) __VA_ARGS__);                                                              \
  } while (0)

#define DPC_FATAL_IF(cond, fstr, ...)                                                                                  \
  do {                                                                                                                 \
    if (cond) DPC_FATAL(fstr __VA_OPT__(, ) __VA_ARGS__);                                                              \
  } while (0)

#define DPC_CHECK(cond, fstr, ...) DPC_FATAL_IF(!(cond), fstr __VA_OPT__(, ) __VA_ARGS__)

#define DPC_UNREACHABLE(...) DPC_FATAL("unreachable" __VA_OPT__(": " __VA_ARGS__))

#endif // !DPC_UTIL_ERROR_H

#ifndef DPC_UTIL_ERROR_H
#define DPC_UTIL_ERROR_H

#include "dpc/util/log.h"
#include <stdexcept>

#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif
// auto fullfmt = fmt::format("dpc.assert {}:{}: {}", __FILE_NAME__, __LINE__, (fstr));

#ifndef NDEBUG
#define DPC_ASSERT(pred, fstr, ...)                                                                                    \
  do { ... } while (0);
#else
#define DPC_ASSERT(pred, fstr, ...)                                                                                    \
  do {                                                                                                                 \
    if (!(pred)) {                                                                                                     \
      auto msg = fmt::format("{}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                      \
      fmt::println(stderr, "dpc.assert:  {}", msg);                                                                    \
      std::quick_exit(1); /*std::quick_exit(1);*/                                                                            \
    }                                                                                                                  \
  } while (0);

#endif

#define DPC_FATAL(fstr, ...)                                                                                           \
  do {                                                                                                                 \
    fmt::println(stderr, "dpc.fatal {}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                \
    std::quick_exit(1);                                                                                                    \
  } while (0)

#define DPC_ERROR(fstr, ...)                                                                                           \
  do {                                                                                                                 \
    auto msg = fmt::format("{}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                        \
    throw std::runtime_error(msg);                                                                                     \
  } while (0)

#define DPC_ERROR_IF(cond, fstr, ...)                                                                                  \
  do {                                                                                                                 \
    if (cond) DPC_ERROR(fstr __VA_OPT__(, ) __VA_ARGS__);                                                              \
  } while (0)

#define DPC_FATAL_IF(cond, fstr, ...)                                                                                  \
  do {                                                                                                                 \
    if (cond) DPC_FATAL(fstr __VA_OPT__(, ) __VA_ARGS__);                                                              \
  } while (0)

#define DPC_UNREACHABLE(...) DPC_FATAL("unreachable" __VA_OPT__(": " __VA_ARGS__))

#endif // !DPC_UTIL_ERROR_H
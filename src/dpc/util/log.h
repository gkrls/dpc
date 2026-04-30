#ifndef DPC_UTIL_LOG_H
#define DPC_UTIL_LOG_H

#include <atomic>
#include <cstdlib>
#include <fmt/format.h>
#include <mutex>
#include <stdexcept>

namespace dpc::log {

enum class LogLevel : int { Trace, Debug, Info, Warn, Error, Fatal };

namespace detail {
inline std::atomic<LogLevel> level{LogLevel::Info};
}

inline void init() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    if (auto *v = std::getenv("DPC_LOG_LEVEL")) {
      std::string_view s(v);
      if (s == "trace") detail::level = LogLevel::Trace;
      else if (s == "debug") detail::level = LogLevel::Debug;
      else if (s == "info") detail::level = LogLevel::Info;
      else if (s == "warn") detail::level = LogLevel::Warn;
    }
  });
}

#define DPC_LOG(lvl, tag, fstr, ...)                                                                                   \
  do {                                                                                                                 \
    if (lvl >= log::detail::level.load(std::memory_order_relaxed))                                                     \
      fmt::println(stderr, "{} {}:{}: " fstr, tag, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                \
  } while (0)

#ifdef DPC_ENABLE_TRACE
#define DPC_TRACE(fstr, ...) DPC_LOG(LogLevel::Trace, "TRACE", fstr __VA_OPT__(, ) __VA_ARGS__)
#else
#define DPC_TRACE(fstr, ...) ((void)0)
#endif

#define DPC_DEBUG(fstr, ...) DPC_LOG(log::LogLevel::Debug, "DEBUG", fstr __VA_OPT__(, ) __VA_ARGS__)
#define DPC_INFO(fstr, ...) DPC_LOG(log::LogLevel::Info, "INFO", fstr __VA_OPT__(, ) __VA_ARGS__)
#define DPC_WARN(fstr, ...) DPC_LOG(log::LogLevel::Warn, "WARN", fstr __VA_OPT__(, ) __VA_ARGS__)

#define DPC_ERROR(fstr, ...)                                                                                           \
  do {                                                                                                                 \
    auto msg = fmt::format("{}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                        \
    fmt::println(stderr, "ERROR {}", msg);                                                                             \
    throw std::runtime_error(msg);                                                                                     \
  } while (0)

#define DPC_FATAL(fstr, ...)                                                                                           \
  do {                                                                                                                 \
    fmt::println(stderr, "FATAL {}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                    \
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

#endif

} // namespace dpc::log

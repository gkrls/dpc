#ifndef DPC_UTIL_LOG_H
#define DPC_UTIL_LOG_H

#include <cstdlib>
#include <fmt/format.h>

namespace dpc::log {

enum class LogLevel : int { Trace, Debug, Info, Warn };

namespace detail {
inline const LogLevel level = [] {
  auto *v = std::getenv("DPC_LOG_LEVEL");
  if (!v) v = std::getenv("DPC_LOG");
  if (!v) return LogLevel::Info;
  std::string_view s(v);
  if (s == "trace") return LogLevel::Trace;
  if (s == "debug") return LogLevel::Debug;
  if (s == "warn") return LogLevel::Warn;
  return LogLevel::Info;
}();
} // namespace detail
} // namespace dpc::log

// #define DPC_LOG(lvl, tag, fstr, ...)                                                                                   \
//   do {                                                                                                                 \
//     if (lvl >= log::detail::level)                                                                                     \
//       fmt::println(stderr, "{} {}:{}: " fstr, tag, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                \
//   } while (0)

#define DPC_LOG(lvl, tag, fstr, ...)                                                                                   \
  do {                                                                                                                 \
    if (lvl >= dpc::log::detail::level) {                                                                                   \
      if (lvl <= dpc::log::LogLevel::Debug)                                                                                 \
        fmt::println(stderr, "{} {}:{}: " fstr, tag, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);              \
      else fmt::println(stderr, "{} " fstr, tag __VA_OPT__(, ) __VA_ARGS__);                                           \
    }                                                                                                                  \
  } while (0)

#ifdef DPC_TRACE_ENABLED
#define DPC_TRACE(fstr, ...) DPC_LOG(dpc::log::LogLevel::Trace, "dpc.trace:", fstr __VA_OPT__(, ) __VA_ARGS__)
#else
#define DPC_TRACE(fstr, ...) ((void)0)
#endif

#define DPC_DEBUG(fstr, ...) DPC_LOG(dpc::log::LogLevel::Debug, "dpc.debug:", fstr __VA_OPT__(, ) __VA_ARGS__)
#define DPC_INFO(fstr, ...) DPC_LOG(dpc::log::LogLevel::Info, "dpc:", fstr __VA_OPT__(, ) __VA_ARGS__)
#define DPC_WARN(fstr, ...) DPC_LOG(dpc::log::LogLevel::Warn, "dpc.warn:", fstr __VA_OPT__(, ) __VA_ARGS__)



#endif // !DPC_UTIL_LOG_H
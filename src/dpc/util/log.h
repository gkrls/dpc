#ifndef DPC_UTIL_LOG_H
#define DPC_UTIL_LOG_H

#include <cstdlib>
#include <fmt/format.h>

namespace dpc::log {

enum class LogLevel : int { Trace, Debug, Info, Warn, Error, Fatal };

namespace detail {
inline const LogLevel level = [] {
  auto *v = std::getenv("DPC_LOG_LEVEL");
  if (!v) return LogLevel::Info;
  std::string_view s(v);
  if (s == "trace") return LogLevel::Trace;
  if (s == "debug") return LogLevel::Debug;
  if (s == "warn") return LogLevel::Warn;
  if (s == "error") return LogLevel::Error;
  return LogLevel::Info;
}();
} // namespace detail

#define DPC_LOG(lvl, tag, fstr, ...)                                                                                   \
  do {                                                                                                                 \
    if (lvl >= log::detail::level)                                                                                     \
      fmt::println(stderr, "{} {}:{}: " fstr, tag, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                \
  } while (0)

#ifdef DPC_ENABLE_TRACE
#define DPC_TRACE(fstr, ...) DPC_LOG(LogLevel::Trace, "dpc.trace:", fstr __VA_OPT__(, ) __VA_ARGS__)
#else
#define DPC_TRACE(fstr, ...) ((void)0)
#endif

#define DPC_DEBUG(fstr, ...) DPC_LOG(log::LogLevel::Debug, "dpc.debug:", fstr __VA_OPT__(, ) __VA_ARGS__)
#define DPC_INFO(fstr, ...) DPC_LOG(log::LogLevel::Info, "dpc:", fstr __VA_OPT__(, ) __VA_ARGS__)
#define DPC_WARN(fstr, ...) DPC_LOG(log::LogLevel::Warn, "dpc.warn:", fstr __VA_OPT__(, ) __VA_ARGS__)

} // namespace dpc::log

#endif // !DPC_UTIL_LOG_H
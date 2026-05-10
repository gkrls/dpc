#ifndef DPC_UTIL_ENV_H
#define DPC_UTIL_ENV_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <string_view>

namespace dpc::env {

// Pure parsers — no side effects, safe to call anywhere

inline std::optional<bool> getbool(const char *name) {
  const char *e = std::getenv(name);
  if (!e) return std::nullopt;
  std::string_view v(e);
  if (v == "1" || v == "ON" || v == "on") return true;
  if (v == "0" || v == "OFF" || v == "off") return false;
  return std::nullopt;
}

inline std::optional<unsigned> getuint(const char *name, std::initializer_list<unsigned> allowed = {}) {
  const char *e = std::getenv(name);
  if (!e) return std::nullopt;
  char *end;
  long res = std::strtol(e, &end, 10);
  if (end == e || res < 0) return std::nullopt;
  if (allowed.size() && std::find(allowed.begin(), allowed.end(), static_cast<unsigned>(res)) == allowed.end())
    return std::nullopt;
  return static_cast<unsigned>(res);
}

inline std::optional<int> getint(const char *name, std::initializer_list<int> allowed = {}) {
  const char *e = std::getenv(name);
  if (!e) return std::nullopt;
  char *end;
  long res = std::strtol(e, &end, 10);
  if (end == e) return std::nullopt;
  if (allowed.size() && std::find(allowed.begin(), allowed.end(), static_cast<int>(res)) == allowed.end())
    return std::nullopt;
  return static_cast<int>(res);
}

inline std::optional<std::string_view> getstr(const char *name, std::initializer_list<std::string_view> allowed = {}) {
  const char *e = std::getenv(name);
  if (!e) return std::nullopt;
  std::string_view v(e);
  if (!allowed.size()) return v;
  auto iequals = [](std::string_view a, std::string_view b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) {
      return std::tolower((unsigned char)c1) == std::tolower((unsigned char)c2);
    });
  };
  for (auto a : allowed)
    if (iequals(v, a)) return std::optional<std::string_view>{a};
  return std::nullopt;
}

// Logging wrapper

namespace detail {
template <typename T> T env_with_log(const char *name, T result) {
  const char *e = std::getenv(name);
  if (!e) return result;
  if (result) printf("dpc: %s set by environment to %s\n", name, e);
  else printf("dpc: %s=%s not valid, ignoring environment\n", name, e);
  return result;
}
} // namespace detail

} // namespace dpc::env

// =====================================================================
// Public Macros — parse + log once at static init
// =====================================================================

#define DPC_ENV_BOOL(VAR, ENV)                                                                                         \
  static const auto VAR = dpc::env::detail::env_with_log(ENV, dpc::env::detail::getbool(ENV))
#define DPC_ENV_UINT(VAR, ENV, ...)                                                                                    \
  static const auto VAR = dpc::env::detail::env_with_log(ENV, dpc::env::getuint(ENV, {__VA_ARGS__}))
#define DPC_ENV_INT(VAR, ENV, ...)                                                                                     \
  static const auto VAR = dpc::env::detail::env_with_log(ENV, dpc::env::getint(ENV, {__VA_ARGS__}))
#define DPC_ENV_STR(VAR, ENV, ...)                                                                                     \
  static const auto VAR = dpc::env::detail::env_with_log(ENV, dpc::env::getstr(ENV, {__VA_ARGS__}))

#endif // !DPC_UTIL_ENV_H
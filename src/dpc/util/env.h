#ifndef DPC_UTIL_ENV_H
#define DPC_UTIL_ENV_H

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <optional>
#include <unordered_set>
#include <string>

namespace dpc::env {

namespace detail {

// Iterate names in order, return env value of first one that's set (or nullptr).
// `matched` gets set to the name that matched.
inline const char *first_set(std::initializer_list<const char *> names, const char *&matched) {
  for (const char *n : names) {
    if (const char *e = std::getenv(n)) {
      matched = n;
      return e;
    }
  }
  matched = nullptr;
  return nullptr;
}

inline bool should_log(const char *name) {
  static std::unordered_set<std::string> logged;
  return logged.insert(name).second;
}

// inline void log_ok(const char *name, const char *value) { printf("dpc: %s set by environment to %s\n", name, value);
// }
inline void log_ok(const char *name, const char *value) {
  if (should_log(name)) printf("dpc: %s set by environment to %s\n", name, value);
}

inline void log_bad(const char *name, const char *value) {
  if (should_log(name)) printf("dpc: %s=%s not valid, ignoring environment\n", name, value);
}

// inline void log_bad(const char *name, const char *value) {
//   printf("dpc: %s=%s not valid, ignoring environment\n", name, value);
// }

inline bool equals_ignore_case(std::string_view a, std::string_view b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                    [](char x, char y) { return std::tolower((unsigned char)x) == std::tolower((unsigned char)y); });
}

} // namespace detail

inline std::optional<bool> getbool(std::initializer_list<const char *> names) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;

  std::string_view v(e);
  if (v == "1" || detail::equals_ignore_case(v, "on") || detail::equals_ignore_case(v, "true")) {
    detail::log_ok(matched, e);
    return true;
  }
  if (v == "0" || detail::equals_ignore_case(v, "off") || detail::equals_ignore_case(v, "false")) {
    detail::log_ok(matched, e);
    return false;
  }
  detail::log_bad(matched, e);
  return std::nullopt;
}

inline std::optional<uint64_t> getuint(std::initializer_list<const char *> names,
                                       std::initializer_list<uint64_t> allowed) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;

  char *end;
  long res = std::strtol(e, &end, 10);
  if (end == e || res < 0) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  if (allowed.size() && std::find(allowed.begin(), allowed.end(), static_cast<uint64_t>(res)) == allowed.end()) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  detail::log_ok(matched, e);
  return static_cast<uint64_t>(res);
}

inline std::optional<uint64_t> getuint(std::initializer_list<const char *> names,
                                       uint64_t lo = std::numeric_limits<uint64_t>::min(),
                                       uint64_t hi = std::numeric_limits<uint64_t>::max()) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;
  char *end;
  long res = std::strtol(e, &end, 10);
  if (end == e || res < 0 || static_cast<uint64_t>(res) < lo || static_cast<uint64_t>(res) > hi) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  detail::log_ok(matched, e);
  return static_cast<uint64_t>(res);
}

inline std::optional<int64_t> getint(std::initializer_list<const char *> names,
                                     std::initializer_list<int64_t> allowed) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;

  char *end;
  long res = std::strtol(e, &end, 10);
  if (end == e) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  if (allowed.size() && std::find(allowed.begin(), allowed.end(), static_cast<int64_t>(res)) == allowed.end()) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  detail::log_ok(matched, e);
  return static_cast<int64_t>(res);
}

inline std::optional<int64_t> getint(std::initializer_list<const char *> names,
                                     int64_t lo = std::numeric_limits<int64_t>::min(),
                                     int64_t hi = std::numeric_limits<int64_t>::max()) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;
  char *end;
  long res = std::strtol(e, &end, 10);
  if (end == e || res < lo || res > hi) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  detail::log_ok(matched, e);
  return static_cast<int64_t>(res);
}

inline std::optional<float> getfloat(std::initializer_list<const char *> names,
                                     float lo = -std::numeric_limits<float>::infinity(),
                                     float hi = std::numeric_limits<float>::infinity()) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;
  char *end;
  float res = std::strtof(e, &end);
  if (end == e || res < lo || res > hi) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  detail::log_ok(matched, e);
  return res;
}

inline std::optional<std::string> getstr(std::initializer_list<const char *> names,
                                         std::initializer_list<const std::string> allowed = {}) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) { return std::nullopt; }

  std::string v(e);
  if (allowed.size()) {
    for (auto a : allowed) {
      if (detail::equals_ignore_case(v, a)) {
        detail::log_ok(matched, e);
        return a;
      }
    }
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  detail::log_ok(matched, e);
  return v;
}

} // namespace dpc::env

#endif // !DPC_UTIL_ENV_H

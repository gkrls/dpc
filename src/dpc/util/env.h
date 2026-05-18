#ifndef DPC_UTIL_ENV_H
#define DPC_UTIL_ENV_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <optional>
// #include <string_view>

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

inline void log_ok(const char *name, const char *value) { printf("dpc: %s set by environment to %s\n", name, value); }

inline void log_bad(const char *name, const char *value) {
  printf("dpc: %s=%s not valid, ignoring environment\n", name, value);
}

inline bool iequals(std::string_view a, std::string_view b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                    [](char x, char y) { return std::tolower((unsigned char)x) == std::tolower((unsigned char)y); });
}

} // namespace detail

inline std::optional<bool> getbool(std::initializer_list<const char *> names) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;

  std::string_view v(e);
  if (v == "1" || detail::iequals(v, "on") || detail::iequals(v, "true")) {
    detail::log_ok(matched, e);
    return true;
  }
  if (v == "0" || detail::iequals(v, "off") || detail::iequals(v, "false")) {
    detail::log_ok(matched, e);
    return false;
  }
  detail::log_bad(matched, e);
  return std::nullopt;
}

inline std::optional<unsigned> getuint(std::initializer_list<const char *> names,
                                       std::initializer_list<unsigned> allowed = {}) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;

  char *end;
  long res = std::strtol(e, &end, 10);
  if (end == e || res < 0) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  if (allowed.size() && std::find(allowed.begin(), allowed.end(), static_cast<unsigned>(res)) == allowed.end()) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  detail::log_ok(matched, e);
  return static_cast<unsigned>(res);
}

inline std::optional<int> getint(std::initializer_list<const char *> names, std::initializer_list<int> allowed = {}) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;

  char *end;
  long res = std::strtol(e, &end, 10);
  if (end == e) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  if (allowed.size() && std::find(allowed.begin(), allowed.end(), static_cast<int>(res)) == allowed.end()) {
    detail::log_bad(matched, e);
    return std::nullopt;
  }
  detail::log_ok(matched, e);
  return static_cast<int>(res);
}

inline std::optional<std::string> getstr(std::initializer_list<const char *> names,
                                         std::initializer_list<const std::string> allowed = {}) {
  const char *matched;
  const char *e = detail::first_set(names, matched);
  if (!e) return std::nullopt;

  std::string v(e);
  if (allowed.size()) {
    for (auto a : allowed) {
      if (detail::iequals(v, a)) {
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
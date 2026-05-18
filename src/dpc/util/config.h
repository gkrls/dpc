#ifndef DPC_UTIL_CONFIG_H
#define DPC_UTIL_CONFIG_H

#include "dpc/util/error.h"
#include "fmt/format.h" // IWYU pragma: keep
#include "nlohmann/json.hpp"
#include <stdexcept>

#include <fstream>

namespace dpc {
namespace conf {

template <typename T> bool read_if_present(T &out, const nlohmann::json &root, const char *ptr_str) {
  const nlohmann::json::json_pointer ptr{ptr_str};
  if (!root.contains(ptr)) return false;
  try {
    out = root.at(ptr).get<T>();
    return true;
  } catch (const nlohmann::json::exception &e) {
    throw std::runtime_error(fmt::format("config parse error at {} (expected {}) : {}", ptr.to_string(),
                                         typeid(T).name(), std::string(e.what())));
  }
}

inline nlohmann::json load_json(const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open()) DPC_ERROR("Failed to open json config '{}'", path);
  return nlohmann::json::parse(f);
}

} // namespace conf
} // namespace dpc

#endif // !DPC_UTIL_CONFIG_H
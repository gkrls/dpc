#ifndef DPC_UTIL_CONFIG_H
#define DPC_UTIL_CONFIG_H

#include "dpc/util/error.h"

#include "fmt/format.h" // IWYU pragma: keep
#include "nlohmann/json.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace dpc {
namespace conf {

// template <typename T> bool json_read_if_present(T &out, const nlohmann::json &root, const char *ptr_str) {
//   const nlohmann::json::json_pointer ptr{ptr_str};
//   if (!root.contains(ptr)) return false;
//   try {
//     out = root.at(ptr).get<T>();
//     return true;
//   } catch (const nlohmann::json::exception &e) {
//     throw std::runtime_error(fmt::format("config parse error at {} (expected {}) : {}", ptr.to_string(),
//                                          typeid(T).name(), std::string(e.what())));
//   }
// }
//
template <typename T> bool json_read_if_present(T &out, const nlohmann::json &j, const char *key) {
  if (!j.contains(key)) return false;
  try {
    out = j.at(key).get<T>();
    return true;
  } catch (const nlohmann::json::exception &e) {
    throw std::runtime_error(
        fmt::format("config parse error at {} (expected {}) : {}", key, typeid(T).name(), std::string(e.what())));
  }
}

inline nlohmann::json json_load(const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open()) DPC_ERROR("Failed to open json config '{}'", path);
  try {
    return nlohmann::json::parse(f);
  } catch (const std::exception &e) { DPC_ERROR("parse error: {}", e.what()); }
}

inline nlohmann::json json_load_at(const std::string &path, const std::string &ptr = "") {
  nlohmann::json root = json_load(path);
  if (ptr.empty()) return root;
  const nlohmann::json::json_pointer p{ptr};
  if (!root.contains(p)) DPC_ERROR("pointer '{}' not found in config '{}'", ptr, path);
  return root.at(p);
}

inline nlohmann::json json_load_key(const std::string &path, const std::string &key = "") {
  std::ifstream f(path);
  if (!f.is_open()) DPC_ERROR("Failed to open json config '{}'", path);
  try {
    nlohmann::json root = nlohmann::json::parse(f);
    if (key.empty()) return root;
    if (!root.contains(key)) DPC_ERROR("key '{}' not found in config '{}'", key, path);
    return root.at(key);
  } catch (const std::exception &e) { DPC_ERROR("parse error: {}", e.what()); }
}

inline nlohmann::json json_load_ensure_key(const std::string &path, const std::string &key) {
  nlohmann::json root = json_load(path);
  if (!root.contains(key)) DPC_ERROR("key '{}' not found in config '{}'", key, path);
  return root;
}

inline nlohmann::json json_load_ensure_keys(const std::string &path, const std::vector<std::string> &keys) {
  nlohmann::json root = json_load(path);
  for (const auto &key : keys) {
    if (!root.contains(key)) DPC_ERROR("key '{}' not found in config '{}'", key, path);
  }
  return root;
}

} // namespace conf
} // namespace dpc

#define OPT(_json, _conf, _opt) dpc::conf::json_read_if_present(_conf._opt, _json, #_opt)


#endif // !DPC_UTIL_CONFIG_H

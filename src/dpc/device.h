#ifndef DPC_DEVICE_H
#define DPC_DEVICE_H

#include "dpc/util/config.h"

// #include "nlohmann/json_fwd.hpp"

#include <cstdint>
#include <string>

namespace dpc {

/// @brief Configuration for a device
///
/// For now it just stores everything in one place
/// TODO: When adding new fields, remember to update the fromJson(json,conf) method
struct DeviceConfig {

  std::string name = "generic-pipelined-switch";
  /// @brief "Fake" mac address of the switch endpoint
  std::string mac = "00:00:00:00:00:00";
  /// @brief "Fake" ip address of the switch endpoint
  std::string addr = "0.0.0.0";
  /// @brief "Fake" udp port of the switch endpoint
  uint16_t port = 4242;
  /// @brief Maximum number of switch pipes available
  uint16_t pipes = 4;
  /// @brief Maximum number of exponent reducers available
  uint16_t exponents = 1;
  /// @brief Maximum number of values reducers available
  uint16_t reducers = 32;
  /// @brief Number of values per reducer
  uint16_t reducer_mode = 2;
  /// @brief Maximum number of slots available
  uint16_t reducer_slots = 32768;
  /// @brief Width of the value in bytes
  uint16_t value_width = 4;
  /// @brief Minimum world size required
  uint16_t world_min = 1;
  /// @brief Maximum world size allowed
  uint16_t world_max = 32;
  /// @brief Maximum number of sessions allowed
  uint16_t sessions_max = 1;
  /// @brief Grpc
  struct ControllerConfig {
    std::string grpc_addr = "";
    uint16_t grpc_port = 50051;
    std::string thrift_addr = "";
    uint16_t thrift_port = 9090;
  } controller{};

  static void fromJson(const nlohmann::json &j, DeviceConfig &c) {
    OPT(j, c, name);
    OPT(j, c, mac);
    OPT(j, c, addr);
    OPT(j, c, port);
    OPT(j, c, pipes);
    OPT(j, c, exponents);
    OPT(j, c, reducers);
    OPT(j, c, reducer_mode);
    OPT(j, c, reducer_slots);
    OPT(j, c, world_min);
    OPT(j, c, world_max);
    OPT(j, c, sessions_max);
  }
  static void fromJson(const std::string &path, DeviceConfig &out) {
    nlohmann::json j = conf::json_load(path);
    fromJson(j, out);
  }
  static DeviceConfig fromJson(const std::string &path) { return fromJson(conf::json_load(path)); }
  static DeviceConfig fromJson(const nlohmann::json &j) {
    DeviceConfig c;
    fromJson(j, c);
    return c;
  }

  // Friendly aliases
  const uint16_t &slots = reducer_slots;

  // Helpers
  inline uint32_t valuesPerPipe() const { return this->reducers * this->reducer_mode; }
  inline uint32_t minValues() const { return this->valuesPerPipe(); }
  inline uint32_t maxValues() const { return this->pipes * this->valuesPerPipe(); }
  inline uint32_t minPayload() const { return this->valuesPerPipe() * value_width; }
  inline uint32_t maxPayload() const { return this->pipes * this->minPayload(); }
  inline bool hasGrpcController() const { return !this->controller.grpc_addr.empty(); }
  inline bool hasThriftController() const { return !this->controller.thrift_addr.empty(); }
  inline bool hasOnlyCliController() const { return this->hasGrpcController() && !this->hasThriftController(); }

  void print(bool detailed = false) const;

  static const DeviceConfig GenericTofino1;
  static const DeviceConfig GenericTofino2;
};

inline const DeviceConfig DeviceConfig::GenericTofino1 = {
    /* name         */ "generic-tofino1",
    /* mac          */ "42:00:00:00:00:00",
    /* addr         */ "42.0.0.1",
    /* port         */ 4242,
    /* pipes        */ 2,
    /* exponents    */ 4,
    /* reducers     */ 32,
    /* reducer_mode */ 2,
    /* reducer_slots*/ 32768,
    /* value_width  */ 4,
    /* world_min    */ 1,
    /* world_max    */ 32,
};

inline const DeviceConfig DeviceConfig::GenericTofino2 = {
    /* name         */ "generic-tofino2",
    /* mac          */ "42:00:00:00:00:00",
    /* addr         */ "42.0.0.1",
    /* port         */ 4242,
    /* pipes        */ 4,
    /* exponents    */ 4,
    /* reducers     */ 32,
    /* reducer_mode */ 2,
    /* reducer_slots*/ 32768,
    /* value_width  */ 4,
    /* world_min    */ 1,
    /* world_max    */ 32,
};

// Represents a session with a ToR switch
// For now this is just a placeholder with static info. In the future, this will
// be dynamically constructed/allocated from a session store (or something)
struct DeviceSession {
  uint32_t id = 1;
  uint32_t pool_base = 0;
  uint32_t pool_size = 2;
  struct Dropsim {
    float ingress = 0.0;
    float egress = 0.0;
  } dropsim;
  static DeviceSession makeDefault(const DeviceConfig &conf) {
    // TODO: allocate dynamically from a session store
    return DeviceSession{1, 0, conf.reducer_slots, {0.0, 0.0}};
  }
};

class Context;

class Device {
public:
  Device(Context &ctx, const DeviceConfig &conf);
  Device(Context &ctx, const DeviceConfig &conf, const DeviceSession &sess);
  DeviceConfig const &config() { return conf; }
  void print(bool detail);
  std::string name() const { return conf.name; }

public:
  Context &ctx;
  const DeviceConfig conf;
  const DeviceSession sess;
};

} // namespace dpc

#endif

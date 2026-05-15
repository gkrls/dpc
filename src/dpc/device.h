#ifndef DPC_DEVICE_H
#define DPC_DEVICE_H


#include <cstdint>
#include <string>

namespace dpc {

struct DeviceConfig {

  std::string name = "generic-switch";
  /// @brief "Fake" mac address of the switch endpoint
  std::string mac = "00:00:00:00:00:00";
  /// @brief "Fake" ip address of the switch endpoint
  std::string addr = "0.0.0.0";
  /// @brief "Fake" udp port of the switch endpoint
  uint16_t port = 4242;
  /// @brief Maximum number of switch pipes available
  uint16_t pipes = 4;
  /// @brief Maximum number of exponent reducers available
  uint16_t exponents = 4;
  /// @brief Maximum number of values reducers available
  uint16_t reducers = 32;
  /// @brief Number of values per reducer
  uint16_t reducer_mode = 2;
  /// @brief Maximum number of slots available
  uint16_t slots = 32768;
  /// @brief Grpc
  struct ControllerConfig {
    std::string grpc_addr = "";
    uint16_t grpc_port = 50051;
    std::string thrift_addr = "";
    uint16_t thrift_port = 9090;
  } controller {};
  struct SessionConfig {
    uint32_t id = 1;
    struct PoolConfig {
      uint32_t base = 0;
      uint32_t size = 2;
    } pool;
    float dropsimIngress = 0; // %
    float dropsimEgress = 0;  // %
  } session {};

  inline uint32_t valuesPerPipe() const { return this->reducers * this->reducer_mode; }
  inline uint32_t minValues() const { return this->valuesPerPipe(); }
  inline uint32_t maxValues() const { return this->pipes * this->valuesPerPipe(); }
  void print(bool detailed = false) const;
public:
  static DeviceConfig fromJson(const std::string &path);

  static const DeviceConfig GenericTofino1;
  static const DeviceConfig GenericTofino2;
};

inline const DeviceConfig DeviceConfig::GenericTofino1 {
  .name = "generic-tofino1",
  .mac = "42:00:00:00:00:00",
  .addr = "42.0.0.1",
  .port = 4242,
  .pipes = 2,
  .exponents = 4,
  .reducers = 32,
  .reducer_mode = 2,
  .slots = 32768,
  .session = {
    .id = 1,
    .pool = {
      .base = 0,
      .size = 2
    },
    .dropsimIngress = 0,
    .dropsimEgress = 0
  },
};

inline const DeviceConfig DeviceConfig::GenericTofino2 {
  .name = "generic-tofino2",
  .mac = "42:00:00:00:00:00",
  .addr = "42.0.0.1",
  .port = 4242,
  .pipes = 4,
  .exponents = 4,
  .reducers = 32,
  .reducer_mode = 2,
  .slots = 32768,
  .session = {
    .id = 1,
    .pool = {
      .base = 0,
      .size = 2
    },
    .dropsimIngress = 0,
    .dropsimEgress = 0
  },
};


class Device {
public:
  Device(DeviceConfig const& o);
  DeviceConfig const conf;
  DeviceConfig const &config() { return conf; }
  void print(bool detail);
};

} // namespace dpc

#endif
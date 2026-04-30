#ifndef DPC_DEVICE_H
#define DPC_DEVICE_H


#include <cstdint>
#include <string>

namespace dpc {

struct DeviceOptions {

  std::string name;
  /// @brief "Fake" mac address of the switch endpoint
  std::string mac = "00:00:00:00:00:00";
  /// @brief "Fake" ip address of the switch endpoint
  std::string ip = "0.0.0.0";
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
  struct Session {
    uint32_t id = 1;
    struct Pool {
      uint32_t base = 0;
      uint32_t size = 2;
    } pool;
    float dropsimIngress = 0; // %
    float dropsimEgress = 0;  // %
  } session;

  inline uint32_t valuesPerPipe() const { return this->reducers * this->reducer_mode; }
  inline uint32_t minValues() const { return this->valuesPerPipe(); }
  inline uint32_t maxValues() const { return this->pipes * this->valuesPerPipe(); }
  void print(bool detailed = false) const;

  static DeviceOptions fromJson(const std::string &path);
};


class Device {
public:
  Device(DeviceOptions const& o);
  DeviceOptions const& opt;

  void print(bool detail);
};

} // namespace dpc

#endif
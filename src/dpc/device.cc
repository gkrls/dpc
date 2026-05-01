#include "dpc/device.h"
#include "dpc/util/error.h"

using namespace dpc;

Device::Device(DeviceConfig const &conf) : conf(conf) {
  if (conf.session.pool.size == 0 || (conf.session.pool.size % 2) != 0)
    DPC_FATAL("session pool size must be non-zero and even");
}


void Device::print(bool detail) {

}
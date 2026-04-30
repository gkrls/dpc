#include "dpc/device.h"
#include "dpc/util/log.h"

using namespace dpc;

Device::Device(DeviceOptions const &o) : opt(o) {
  if (o.session.pool.size == 0 || (o.session.pool.size % 2) != 0)
    DPC_FATAL("session pool size must be non-zero and even");
}


void Device::print(bool detail) {

}
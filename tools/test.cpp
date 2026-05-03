#include "dpc/backend/noop/noop_backend.h"
#include "dpc/context.h"
#include "dpc/device.h"
#include "dpc/types.h"
#include "dpc/util/net_iface.h"

int main(int argc, const char** argv) {

  dpc::NoopConfig cfg(10000, 2);

  dpc::Context ctx(0, 10, dpc::DeviceConfig::DefaultTofinoD, cfg);

  // dpc::net::print_ifaces();
  // ctx.print();

  uint32_t size = 1024;
  uint32_t *data = &size;

  ctx.AllReduce(data, data, size, dpc::DataType::I32, dpc::ReduceOp::Sum);

  return 0;
}
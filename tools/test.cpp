#include "dpc/context.h"
#include "dpc/device.h"

int main(int argc, const char** argv) {
  dpc::Context ctx(0, 10, dpc::DeviceConfig::DefaultTofinoD);

  ctx.print();
  return 0;
}
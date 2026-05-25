#include "dpc/backend/noop/noop_backend.h"
#include "dpc/backend/sock/sock_backend.h"
#include "dpc/context.h"
#include "dpc/device.h"
#include "dpc/task.h"

int main(int, char**) {

  dpc::NoopConfig cfg(1000, 2);

  dpc::Context ctx(0, 10, dpc::DeviceConfig::GenericTofino1, dpc::SockConfig{});

  std::vector<uint32_t> data(1024);

  ctx.AllReduceAsync(&data[0], &data[0], data.size(), dpc::DataType::U32, dpc::ReduceOp::Sum);
  ctx.AllReduceAsync(&data[0], &data[0], data.size(), dpc::DataType::U32);
  ctx.AllReduceAsync(&data[0], &data[0], data.size(), dpc::DataType::U32);
  ctx.AllReduceAsync(&data[0], &data[0], data.size(), dpc::DataType::U32);
  ctx.ReduceScatterAsync(&data[0], &data[0], data.size() / ctx.world, dpc::U32);
  ctx.AllGather(&data[0], &data[0], data.size() / ctx.world, dpc::U32);
  ctx.AllReduce(&data[0], &data[0], data.size(), dpc::DataType::U32);
  // std::this_thread::sleep_for(std::chrono::milliseconds(0));
  return 0;
}

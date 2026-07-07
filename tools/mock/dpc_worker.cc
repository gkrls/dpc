#include "dpc/backend/backend.h"
#include "dpc/task.h"

#include <dpc/context.h>
#include <numeric>

int main(int argc, char **argv) {
  if (argc < 1) return 1;

  auto dev = dpc::DeviceConfig::fromJson(std::string(argv[1]));

  dpc::Context ctx(0, 1, dev, dpc::Backend::Sock);

  std::vector<uint32_t> data(1024);
  std::iota(data.begin(), data.end(), 1);

  ctx.AllReduceAsync(data.data(), data.data(), data.size(), dpc::DataType::U32);
  ctx.AllGather(data.data(), data.data(), data.size(), dpc::DataType::U32);
  ctx.waitAll();
}

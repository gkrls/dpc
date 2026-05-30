#include "dpc/device.h"

#include "dpc/context.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"

using namespace dpc;

Device::Device(Context &ctx, DeviceConfig const &conf) : conf(conf) {
  if (conf.session.pool_size == 0 || (conf.session.pool_size % 2) != 0)
    DPC_FATAL("session pool size must be non-zero and even");
  if (ctx.world < conf.world_min || ctx.world > conf.world_max)
    DPC_FATAL("device {} can only supports world size between {} and {}", conf.name, conf.world_min, conf.world_max);
}

void Device::print(bool detail) {
  DPC_INFO("{}: addr={}:{} pipes={} reducers={}/{} slots={} sess={} pool={}-{}", conf.name, conf.addr, conf.port,
           conf.pipes, conf.reducers, conf.reducer_mode, conf.reducer_slots, conf.session.id, conf.session.pool_base,
           conf.session.pool_base + conf.session.pool_size);
}

// Ctx:
// Dev:
// Bck:

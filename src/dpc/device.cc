#include "dpc/device.h"

#include "dpc/context.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"

#include "device.h"

using namespace dpc;

// Just create device session 1 with pool size the entire memory
// In the future we should handle this dynamically, perhaps use a store or something
Device::Device(Context &ctx, DeviceConfig const &conf) : Device(ctx, conf, DeviceSession::makeDefault(conf)) {}

Device::Device(Context &ctx, DeviceConfig const &conf, DeviceSession const &sess) : ctx(ctx), conf(conf), sess(sess) {
  DPC_FATAL_IF(sess.id == 0, "invalid session id '{}'", sess.id);
  DPC_FATAL_IF(sess.pool_size == 0 || (sess.pool_size % 2) != 0, "session pool size must be non-zero and even");
  DPC_FATAL_IF(sess.pool_base > conf.reducer_slots || (sess.pool_base + sess.pool_size) > conf.reducer_slots,
               "session pool range {}-{} outside device slot range {}-{}", sess.pool_base,
               sess.pool_base + sess.pool_size, 0, conf.reducer_slots);
  DPC_FATAL_IF((ctx.world < conf.world_min) || (ctx.world > conf.world_max),
               "device {} only supports world size between {} and {} (ctx.world={})", conf.name, conf.world_min,
               conf.world_max, ctx.world);
}

void Device::print(bool detail) {
  DPC_INFO("{}: addr={}:{} pipes={} reducers={}/{} slots={} | sess={} pool={}-{}", conf.name, conf.addr, conf.port,
           conf.pipes, conf.reducers, conf.reducer_mode, conf.reducer_slots, sess.id, sess.pool_base,
           sess.pool_base + sess.pool_size);
}

// Ctx:
// Dev:
// Bck:

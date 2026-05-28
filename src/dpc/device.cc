#include "dpc/device.h"

#include "dpc/context.h"
#include "dpc/util/config.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"

#include "nlohmann/json.hpp"

using namespace dpc;

DeviceConfig DeviceConfig::fromJson(const std::string &path) {
  auto j = conf::json_load(path);
  if (j.contains("device")) j = j.at("device");
  DeviceConfig c;
  OPT(j, c, addr);
  OPT(j, c, port);
  OPT(j, c, pipes);
  OPT(j, c, exponents);
  OPT(j, c, reducers);
  OPT(j, c, reducer_mode);
  OPT(j, c, slots);

  if (j.contains("session")) {
    j = j.at("session");
    OPT(j, c.session, id);
    OPT(j, c.session, pool_base);
    OPT(j, c.session, pool_size);
  }
  return c;
}

Device::Device(Context &ctx, DeviceConfig const &conf) : conf(conf) {
  if (conf.session.pool_size == 0 || (conf.session.pool_size % 2) != 0)
    DPC_FATAL("session pool size must be non-zero and even");
  if (ctx.world < conf.world_min || ctx.world > conf.world_max)
    DPC_FATAL("device {} can only supports world size between {} and {}", conf.name, conf.world_min, conf.world_max);
}

void Device::print(bool detail) {
  DPC_INFO("{}: addr={}:{} pipes={} reducers={}/{} slots={} sess={} pool={}-{}", conf.name, conf.addr, conf.port,
           conf.pipes, conf.reducers, conf.reducer_mode, conf.slots, conf.session.id, conf.session.pool_base,
           conf.session.pool_base + conf.session.pool_size);
}

// Ctx:
// Dev:
// Bck:

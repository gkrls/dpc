#include "dpc/device.h"

#include "dpc/util/config.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"

#include "nlohmann/json.hpp"
#include <iostream>

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

Device::Device(DeviceConfig const &conf) : conf(conf) {
  if (conf.session.pool_size == 0 || (conf.session.pool_size % 2) != 0)
    DPC_FATAL("session pool size must be non-zero and even");
  std::cout << conf.name << '\n';
}

void Device::print(bool detail) {
  DPC_INFO("{}: addr={}/{} pipes={} reducers={}/{} slots={} sess={} pool={}-{}", conf.name, conf.addr, conf.port,
           conf.pipes, conf.reducers, conf.reducer_mode, conf.slots, conf.session.id, conf.session.pool_base,
           conf.session.pool_base + conf.session.pool_size);
}

// Ctx:
// Dev:
// Bck:

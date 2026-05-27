#include "process_group_dpc.h"

#include "dpc/util/error.h"

namespace c10d {

static c10::intrusive_ptr<ProcessGroupGloo::Options>
prepareGlooOptions(c10::intrusive_ptr<ProcessGroupGloo::Options> opt) {
  auto opts = opt ? opt : c10d::ProcessGroupGloo::Options::create();

  if (opts->threads < 2) opts->threads = 2;
  if (opts->devices.empty()) {
    const char *ifname = std::getenv("GLOO_SOCKET_IFNAME");
    try {
      if (ifname && *ifname) {
        opts->devices.push_back(c10d::ProcessGroupGloo::createDeviceForInterface(ifname));
      } else {
        opts->devices.push_back(c10d::ProcessGroupGloo::createDefaultDevice());
      }
    } catch (const std::exception &e) { DPC_FATAL("prepareGlooOptions failed to create Gloo device {}", e.what()); }
  }
  return opts;
}

ProcessGroupDPC::ProcessGroupDPC(const DistributedBackendOptions &dist, const Options &options)
    : ProcessGroupGloo(dist.store, dist.group_rank, dist.group_size, prepareGlooOptions(options.gloo)), opts_(options) {

  if (options.backend) {
    dpc_ = std::make_shared<dpc::Context>(dist.group_rank, dist.group_size, options.device, *options.backend);
  } else {
    dpc_ = std::make_shared<dpc::Context>(dist.group_rank, dist.group_size, options.device);
  }

  // pinned pool setup (same as your existing code, if you still need it)
}

static c10::intrusive_ptr<Backend> createProcessGroupDPC(const c10d::DistributedBackendOptions &dist,
                                                         const ProcessGroupDPC::Options &opts) {
  return c10::make_intrusive<ProcessGroupDPC>(dist, opts);
}

} // namespace c10d

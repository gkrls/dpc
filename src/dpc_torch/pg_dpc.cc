#include "pg_dpc.h"

#include "dpc/util/error.h"

namespace dpc {

static c10::intrusive_ptr<c10d::ProcessGroupGloo::Options>
prepareGlooOptions(c10::intrusive_ptr<c10d::ProcessGroupGloo::Options> opt) {
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

ProcessGroupDPC::ProcessGroupDPC(const c10d::DistributedBackendOptions &dist, const Options &options)
    : ProcessGroupGloo(dist.store, dist.group_rank, dist.group_size, prepareGlooOptions(options.gloo)), opts_(options) {

  auto rank = dist.group_rank;
  auto world = dist.group_size;
  if (options.device && options.backend) {
    dpc_ = std::make_shared<dpc::Context>(rank, world, *options.device, *options.backend, options.timeout.count());
  } else if (options.device) {
    dpc_ = std::make_shared<dpc::Context>(rank, world, *options.device, options.timeout.count());
  } else if (options.backend) {
    dpc_ = std::make_shared<dpc::Context>(rank, world, *options.backend, options.timeout.count());
  } else {
    dpc_ = std::make_shared<dpc::Context>(rank, world, options.timeout.count());
  }

  // pinned pool setup (same as your existing code, if you still need it)
}

c10::intrusive_ptr<c10d::Backend> create_process_group_dpc(const c10d::DistributedBackendOptions &dist,
                                                           const ProcessGroupDPC::Options &opts) {
  return c10::make_intrusive<ProcessGroupDPC>(dist, opts);
}

__attribute__((constructor)) static void register_process_group_dpc() {
  py::gil_scoped_acquire acquire;
  try {
    py::object module = py::module::import("torch.distributed");
    py::object register_backend = module.attr("Backend").attr("register_backend");

    // register_backend("dpc", py::cpp_function(dpc::create_process_group_dpc), py::arg("extended_api") = true,
    //                  py::arg("devices") = py::make_tuple("cpu", "cuda"));

    register_backend(
        "dpc",
        py::cpp_function(
            [](const c10d::DistributedBackendOptions &dist,
               c10::intrusive_ptr<dpc::ProcessGroupDPC::Options> opts) -> c10::intrusive_ptr<c10d::Backend> {
              if (!opts) opts = c10::make_intrusive<dpc::ProcessGroupDPC::Options>();
              return dpc::create_process_group_dpc(dist, *opts);
            }),
        py::arg("extended_api") = true, py::arg("devices") = py::make_tuple("cpu", "cuda"));
  } catch (const py::error_already_set &e) { std::cerr << "Error registering DPC backend: " << e.what() << std::endl; }
}

} // namespace dpc

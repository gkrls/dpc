#ifndef DPC_TORCH_PROCESS_GROUP_DPC_H
#define DPC_TORCH_PROCESS_GROUP_DPC_H

#include "dpc/backend/backend.h"
#include "dpc/context.h"

#define USE_C10D_GLOO
#include "torch/csrc/distributed/c10d/ProcessGroupGloo.hpp"
#include "torch/extension.h"

namespace c10d {

constexpr const char *DPC_BACKEND_NAME = "dpc";

class ProcessGroupDPC : public ProcessGroupGloo {
public:
  class Options : public Backend::Options {
  public:
    c10::intrusive_ptr<ProcessGroupGloo::Options> gloo = nullptr;
    dpc::DeviceConfig device;
    std::shared_ptr<dpc::BackendConfig> backend = nullptr;
    size_t hint_pinned_tensor_size = 0;
    size_t hint_pinned_tensor_pool_size = 0;
    Options() : Backend::Options(DPC_BACKEND_NAME) {}
  };

public:
  ProcessGroupDPC(const c10d::DistributedBackendOptions &dist, const Options &opts);

private:
  Options opts_;
  std::shared_ptr<dpc::Context> dpc_ = nullptr;
  // std::shared_ptr<PinnedMempool> pinned_pool_ = nullptr;
};

} // namespace c10d

#endif // DPC_TORCH_PROCESS_GROUP_DPC_H

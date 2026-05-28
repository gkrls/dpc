#ifndef DPC_TORCH_PROCESS_GROUP_DPC_H
#define DPC_TORCH_PROCESS_GROUP_DPC_H

#include "dpc/backend/backend.h"
#include "dpc/context.h"
#include "dpc/device.h"

#include "torch/extension.h" // IWYU pragma: keep


#define USE_C10D_GLOO
#include "torch/csrc/distributed/c10d/ProcessGroupGloo.hpp"

namespace dpc {
constexpr const char *DPC_BACKEND_NAME = "dpc";
class ProcessGroupDPC : public c10d::ProcessGroupGloo {
public:
  class Options : public c10d::Backend::Options {
  public:
    c10::intrusive_ptr<c10d::ProcessGroupGloo::Options> gloo = nullptr;
    std::shared_ptr<dpc::DeviceConfig> device = nullptr;
    std::shared_ptr<dpc::BackendConfig> backend = nullptr;
    size_t hint_pinned_tensor_size = 0;
    size_t hint_pinned_tensor_pool_size = 0;
    Options() : c10d::Backend::Options(DPC_BACKEND_NAME) {}
  };

public:
  ProcessGroupDPC(const c10d::DistributedBackendOptions &dist, const Options &opts);
  c10::intrusive_ptr<c10d::Work> allreduce(std::vector<at::Tensor> &tensors,
                                           const c10d::AllreduceOptions &opts = c10d::AllreduceOptions()) override;

private:
  Options opts_;
  std::shared_ptr<dpc::Context> dpc_ = nullptr;
};

c10::intrusive_ptr<c10d::Backend> create_process_group_dpc(const c10d::DistributedBackendOptions &dist,
                                                           const ProcessGroupDPC::Options &opts);
} // namespace dpc

#endif // DPC_TORCH_PROCESS_GROUP_DPC_H

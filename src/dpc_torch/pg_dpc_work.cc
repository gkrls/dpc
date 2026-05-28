#include "pg_dpc.h"

using namespace dpc;

c10::intrusive_ptr<c10d::Work> ProcessGroupDPC::allreduce(std::vector<at::Tensor> &tensors,
                                                          const c10d::AllreduceOptions &opts) {
  TORCH_CHECK(false, "ProcessGroupDPC::allreduce not yet implemented");
}

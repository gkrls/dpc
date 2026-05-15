#include "dpc/backend/dpdk/dpdk_backend.h"

using namespace dpc;

DpdkBackend::DpdkBackend(Context &ctx, const DpdkConfig &conf) : Backend(ctx, Backend::Dpdk), conf(conf) {}

DpdkBackend::~DpdkBackend() {}

void DpdkBackend::print(bool details) const {
  DPC_INFO("BCK: dpdk");
}

bool DpdkBackend::supports(Collective c, DataType ty) const { return c == Collective::AllReduce; }

void DpdkBackend::push(std::shared_ptr<Task> task) { }
void DpdkBackend::start() {}
void DpdkBackend::stop() {}
void DpdkBackend::notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status) {}
void DpdkBackend::configDpdkPort() {}
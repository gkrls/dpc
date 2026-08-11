#include "dpc/backend/backend.h"
#include "dpc/backend/sock/sock_backend.h"
#include "dpc/context.h"
#include "dpc/task.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"
#include "dpc/util/sys.h"

#include <chrono>
#include <thread>

using namespace dpc;

SockWorker::SockWorker(SockBackend &backend, uint16_t id, int pin_core)
    : BackendWorker(backend, id), backend_(backend), conf_(backend.config()),
      net_(id, conf_, backend.context().device().config()), pin_core_(pin_core), thread_(&SockWorker::main, this) {}

SockWorker::~SockWorker() {
  DPC_CHECK(!thread_.joinable(), "SockWorker destroyed without stop()+join()");
  stop(true);
}

void SockWorker::join() {
  if (thread_.joinable()) thread_.join();
}

void SockWorker::main() {
  // Run any Socket specific initialization
  if (pin_core_ >= 0) {
    DPC_DEBUG("pinning worker to core {}", pin_core_);
    sys::pin_to_core(pin_core_);
  }
  // Run the main task loop
  BackendWorker::main();
}

Task::Status SockWorker::execute(std::shared_ptr<Task> task) {
  DPC_INFO("running task {}", task->name);
  switch (task->coll) {
  default: DPC_ERROR("unsupported collective operation: {}", dpc::getCollectiveName(task->coll));
  case Collective::AllReduce: return allreduce(task);
  case Collective::AllGather: return allgather(task);
  }
}

Task::Status SockWorker::allreduce(std::shared_ptr<Task> task) {
  DPC_INFO("allreduce task {} running...", task->name);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  DPC_INFO("allreduce task {} done", task->name);
  return Task::Status::Completed;
}

Task::Status SockWorker::allgather(std::shared_ptr<Task> task) {
  DPC_INFO("allgather task {} running...", task->name);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  DPC_INFO("allgather task {} done", task->name);
  return Task::Status::Completed;
}

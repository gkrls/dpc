#include "dpc/backend/sock/sock_backend.h"
#include "dpc/context.h"
#include "dpc/util/cpu.h"
#include "dpc/util/error.h"

using namespace dpc;

SockWorker::SockWorker(SockBackend &backend, uint16_t id, int pin_core)
    : BackendWorker(backend, id), backend_(backend), conf_(backend.config()),
      net_(id, conf_, backend.context().device().config()), pin_core_(pin_core), thread_(&SockWorker::run, this) {}

SockWorker::~SockWorker() {
  DPC_CHECK(!thread_.joinable(), "SockWorker destroyed without stop()+join()");
  stop(true);
}

void SockWorker::run() {
  if (pin_core_ >= 0) cpu::pin_to_core(pin_core_);
  main();
}

Task::Status SockWorker::execute(std::shared_ptr<Task> task) { return Task::Status::Completed; }

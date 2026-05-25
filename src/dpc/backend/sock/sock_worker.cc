#include "dpc/backend/sock/sock_backend.h"
#include "dpc/context.h"

using namespace dpc;

SockWorker::SockWorker(uint16_t id, SockBackend &backend, const SockConfig &conf)
    : BackendWorker(id, backend), net_(id, conf, backend.context().device().config()) {}

Task::Status SockWorker::execute(std::shared_ptr<Task> task) { return Task::Status::Completed; }

#include "dpc/backend/sock/sock_backend.h"

using namespace dpc;

SockBackend::SockBackend(Context &ctx, const SockConfig &conf) : Backend(ctx, Backend::Sock) {
  // TODO
}

SockBackend::~SockBackend() {
  // TODO
}

void SockBackend::start() {
  // TODO
}

void SockBackend::stop() {
  // TODO
};

void SockBackend::push(std::shared_ptr<Task> task) {
  DPC_CHECK(state_ == Running, "backend is not in Running state. Make sure start() is called before push()");
  DPC_CHECK(task->getStatus() == Task::Created, "task {} already submitted to backend", task->name);
  {
    std::lock_guard<std::mutex> lock(work_mutex);
    auto [it, inserted] = work.try_emplace(task->id, TaskState{conf.threads, Task::Completed});
    DPC_CHECK(inserted, "attempted to push task {} more than once", task->name);
  }
  task->setStatus(Task::Submitted);
  for (auto &worker : workers) worker->push(task);
}

void SockBackend::print(bool details) const {
  // TODO
}


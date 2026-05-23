#include "dpc/backend/backend.h"

#include "dpc/backend/noop/noop_backend.h"
#include "dpc/backend/sock/sock_backend.h"

#include <string_view>
#if DPC_DPDK_ENABLED
#include "dpc/backend/dpdk/dpdk_backend.h"
#endif
#include "dpc/util/error.h"

#include "nlohmann/json.hpp"

#include <memory>

using namespace dpc;

using nlohmann::json;

// ============= BACKEND REGISTRATION =============
const std::vector<Backend::Entry> Backend::registry = {
    {Noop, "noop", &make_backend<NoopBackend, NoopConfig>, &make_config<NoopConfig>},
    {Sock, "sock", &make_backend<SockBackend, SockConfig>, &make_config<SockConfig>},
#if DPC_DPDK_ENABLED
    {Dpdk, "dpdk", &make_backend<DpdkBackend, DpdkConfig>, &make_config<DpdkConfig>},
#endif
};
// ============= BACKEND REGISTRATION =============

std::unique_ptr<BackendConfig> BackendConfig::fromJson(const std::string &path) {
  auto data = conf::load_json(path);
  if (!data.is_object()) DPC_ERROR("config {} must be a json object", path);

  for (auto it = data.begin(); it != data.end(); ++it) {
    for (auto &e : Backend::registry) {
      if (it.key() == e.name) return e.make_config(path);
    }
  }
  DPC_ERROR("config {} contains no recognized backend", path);
}

std::unique_ptr<BackendConfig> BackendConfig::fromJson(const std::string &path, Backend::Kind kind) {
  auto data = conf::load_json(path);
  if (!data.is_object()) DPC_ERROR("config {} must be a json object", path);
  for (auto &e : Backend::registry) {
    if (e.kind != kind) continue;
    if (!data.contains(e.name)) DPC_ERROR("config {} does not contain '{}' section", path, e.name);
    return e.make_config(path);
  }
  DPC_ERROR("unhandled backend kind in {}", path);
}

std::unique_ptr<BackendConfig> BackendConfig::get(Backend::Kind kind) {
  for (auto &e : Backend::registry)
    if (e.kind == kind) return e.make_config("");
  DPC_UNREACHABLE("unhandled kind");
}

std::unique_ptr<BackendConfig> BackendConfig::get(const std::string &name) {
  for (auto &e : Backend::registry)
    if (e.name == name) return e.make_config("");
  DPC_ERROR("unknown backend {}", name);
}

std::unique_ptr<Backend> Backend::create(Context &ctx, Backend::Kind kind) {
  for (auto &e : registry)
    if (e.kind == kind) return e.make_backend(ctx, *BackendConfig::get(kind));
  DPC_UNREACHABLE("unhandled kind");
}

std::unique_ptr<Backend> Backend::create(Context &ctx, BackendConfig const &conf) {
  for (auto &e : registry)
    if (e.kind == conf.kind_) return e.make_backend(ctx, conf);
  DPC_UNREACHABLE("unhandled kind");
}

std::string Backend::name(Backend::Kind kind) {
  for (auto &e : registry)
    if (e.kind == kind) return e.name;
  DPC_FATAL("internal: unregistered backend kind");
}

Backend::Kind Backend::get(std::string_view name) {
  for (auto &e : registry)
    if (e.name == name) return e.kind;
  DPC_FATAL("internal: unregistered backend name '{}'", name);
}

Backend::Kind Backend::kind(std::string_view name) { return get(name); }

// std::string Backend::getName(BackendConfig const& conf) {
//   return Backend::getName(conf.kind_);
// };

//
// BACKEND WORKER IMPL
//

void BackendWorker::start() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::Init) return;
    state_ = State::Running;
  }
  cv_.notify_one();
}

void BackendWorker::stop(bool join) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::Stopped) return;
    state_ = State::Stopped;
  }
  cv_.notify_one();
  if (join) this->join();
}

void BackendWorker::push(std::shared_ptr<Task> task) {
  DPC_CHECK(state_ == State::Running, "worker-{} is not running", id_);
  queue_.push(task);
  { std::lock_guard<std::mutex> lock(mutex_); }
  cv_.notify_one();
}

void BackendWorker::main() {
  // Phase 1: park until start() or stop().
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return state_ != State::Init; });
    // if (state_ == State::Stopped) return; // stop() before start()
  }

  // Phase 2: process tasks until stop().
  while (true) {
    if (state_ == State::Stopped) break; // check first cause
    std::shared_ptr<Task> task;
    if (queue_.try_pop(task)) {
      backend_.notify(id_, task, Task::Running);
      // on_task_start(task);
      backend_.notify(id_, task, execute(task));
      // on_task_finish(task, execute(task));
      continue;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty() || state_ == State::Stopped; });
  }

  // Phase 3: drain.
  std::shared_ptr<Task> task = nullptr;
  while (queue_.try_pop(task)) backend_.notify(id_, task, Task::Aborted);
}

//
// MULTIWORKER BACKEND IMPLEMENTATION
//

void MultiworkerBackend::start() {
  std::lock_guard<std::mutex> lock(state_mutex);
  if (state_ != State::Init) return;
  state_ = State::Running;
  for (auto &w : workers) w->start();
}

void MultiworkerBackend::stop() {
  std::lock_guard<std::mutex> lock(state_mutex);
  if (state_ >= State::Stopping) return;
  state_ = State::Stopping;
  for (auto &w : workers) w->stop();
  for (auto &w : workers) w->join();
  state_ = State::Stopped;
}

// void MultiworkerBackend::push(std::shared_ptr<Task> task) {
//   DPC_CHECK(task->getStatus() == Task::Created, "task {} already submitted to backend", task->name);
//   {
//     std::lock_guard<std::mutex> lock(state_mutex);
//     if (state_ != Running) {
//       task->setStatus(Task::Aborted);
//       return;
//     }
//   }
//   {
//     std::unique_lock<std::mutex> lock(work_mutex);
//     auto [it, inserted] =
//         work.try_emplace(task->id, TaskProgress{static_cast<uint16_t>(workers.size()), Task::Completed});
//     DPC_CHECK(inserted, "attempted to push task {} more than once", task->name);
//   }
//   task->setStatus(Task::Submitted);
//   for (auto &worker : workers) worker->push(task);
// }
//
void MultiworkerBackend::push(std::shared_ptr<Task> task) {
  DPC_CHECK(task->getStatus() == Task::Created, "task {} already submitted to backend", task->name);

  std::lock_guard<std::mutex> lock(state_mutex);
  if (state_ != Running) {
    task->setStatus(Task::Aborted);
    return;
  }
  {
    std::lock_guard<std::mutex> wlock(work_mutex);
    auto [it, inserted] =
        work.try_emplace(task->id, TaskProgress{static_cast<uint16_t>(workers.size()), Task::Completed});
    DPC_CHECK(inserted, "attempted to push task {} more than once", task->name);
  }
  task->setStatus(Task::Submitted);
  for (auto &worker : workers) worker->push(task);
}

void MultiworkerBackend::notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status) {
  if (status == Task::Running) {
    task->setStatus(Task::Running);
    return;
  }

  Task::Status final_status;
  bool done = false;
  {
    std::lock_guard<std::mutex> lock(work_mutex);
    auto it = work.find(task->id);
    DPC_CHECK(it != work.end(), "t-{} notify for unknown task {}", tid, task->name);
    DPC_CHECK(it->second.remaining_workers > 0, "t-{} notify for task {} with 0 workers remaining", tid, task->name);

    if (status > it->second.worst) it->second.worst = status;

    if (--it->second.remaining_workers == 0) {
      final_status = it->second.worst;
      work.erase(it);
      done = true;
    }
  }

  if (done) task->setStatus(final_status);
}

#include "dpc/backend/noop/noop_backend.h"

#include "dpc/backend/backend.h"
#include "dpc/context.h"
#include "dpc/task.h"
#include "dpc/util/config.h"
#include "dpc/util/env.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"

#include "nlohmann/json.hpp"

#include <fstream>
#include <memory>
#include <mutex>

using namespace dpc;

namespace {
const auto kThreads = env::getuint({"DPC_NOOP_THREADS"});
const auto kOpMs = env::getuint({"DPC_NOOP_OP_MS"});
} // namespace

NoopConfig NoopConfig::fromJson(const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open()) DPC_ERROR("Failed to open json config '{}'", path);
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(f);
  } catch (const nlohmann::json::parse_error &e) { DPC_ERROR("Parse error in json config {}: {}", path, e.what()); }
  if (!root.is_object()) DPC_ERROR("config {} must be a json object", path);
  if (!root.contains(Backend::name(Backend::Noop))) DPC_ERROR("config '{}' does not contain 'noop'", path);

  NoopConfig c;
  conf::read_if_present(c.threads, root, "/noop/threads");
  conf::read_if_present(c.op_ms, root, "/noop/op_ms");
  return c;
}

NoopBackend::NoopBackend(Context &ctx, NoopConfig const &conf) : MultiworkerBackend(ctx, Backend::Noop), conf(conf) {
  for (auto i = 0; i < conf.threads; ++i) workers.push_back(std::unique_ptr<NoopWorker>(new NoopWorker(i, *this)));
}

// void NoopBackend::start() {
//   std::call_once(start_flag, [this] {
//     state_ = State::Running;
//     for (auto &w : workers) w->start();
//   });
// }

// void NoopBackend::stop() {
//   std::call_once(stop_flag, [this] {
//     state_ = State::Stopping;
//     for (auto &w : workers) w->stop();
//     for (auto &w : workers) w->join();
//     state_ = State::Stopped;
//   });
// }

// DPC_ERROR_IF(state_ != State::Running, "backend.start() is required before backend.push()");
// void NoopBackend::push(std::shared_ptr<Task> task) {
//   DPC_CHECK(state_ == Running, "backend is not in Running state. Make sure start() is called before push()");
//   DPC_CHECK(task->getStatus() == Task::Created, "task {} already submitted to backend", task->name);

//   {
//     std::lock_guard<std::mutex> lock(work_mutex);
//     auto [it, inserted] = work.try_emplace(task->id, TaskProgress{conf.threads, Task::Completed});
//     DPC_CHECK(inserted, "attempted to push task {} more than once", task->name);
//   }
//   task->setStatus(Task::Submitted);
//   for (auto &worker : workers) worker->push(task);
// }

// void NoopBackend::notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status) {
//   if (status == Task::Running) {
//     task->setStatus(Task::Running);
//     return;
//   }

//   std::lock_guard<std::mutex> lock(tasks_mutex);
//   auto it = tasks.find(task->id);
//   if (it == tasks.end()) return;

//   // Promote to worse status if this worker reported worse
//   if (status > it->second.worst) it->second.worst = status;

//   if (--it->second.remaining == 0) {
//     task->setStatus(it->second.worst);
//     tasks.erase(it);
//   }

//   DPC_CHECK(it->second.remaining >= 0, "t-{} finished {} made remaining counter negative ('{}')", tid, task->name,
//             it->second.remaining);
// }
// void NoopBackend::notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status) {
//   if (status == Task::Running) {
//     task->setStatus(Task::Running);
//     return;
//   }

//   std::lock_guard<std::mutex> lock(work_mutex);
//   auto it = work.find(task->id);
//   if (it == work.end()) return;

//   // Promote to worse status if this worker reported worse
//   if (status > it->second.worst) it->second.worst = status;

//   if (--it->second.remaining_workers == 0) {
//     task->setStatus(it->second.worst);
//     work.erase(it);
//   }

//   DPC_CHECK(it->second.remaining_workers >= 0, "t-{} finished {} made remaining counter negative ('{}')", tid,
//             task->name, it->second.remaining_workers);
// }

void NoopBackend::print(bool details) const { DPC_INFO("BCK: {}, workers={}", name(), conf.threads); }

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

using Worker = dpc::NoopBackend::Worker;

NoopBackend::NoopBackend(Context &ctx, NoopConfig const &conf) : Backend(ctx, Backend::Noop), conf(conf) {
  for (auto i = 0; i < conf.threads; ++i) workers.push_back(std::unique_ptr<Worker>(new Worker(i, *this)));
}

void NoopBackend::start() {
  std::call_once(start_flag, [this] {
    state_ = State::Running;
    for (auto &w : workers) w->start();
  });
}

void NoopBackend::stop() {
  std::call_once(stop_flag, [this] {
    state_ = State::Stopping;
    for (auto &w : workers) w->stop();
    for (auto &w : workers) w->join();
    state_ = State::Stopped;
  });
}

// DPC_ERROR_IF(state_ != State::Running, "backend.start() is required before backend.push()");
void NoopBackend::push(std::shared_ptr<Task> task) {
  DPC_CHECK(state_ == Running, "backend is not in Running state. Make sure start() is called before push()");
  DPC_CHECK(task->getStatus() == Task::Created, "task {} already submitted to backend", task->name);

  {
    std::lock_guard<std::mutex> lock(tasks_mutex);
    auto [it, inserted] = tasks.try_emplace(task->id, TaskState{conf.threads, Task::Completed});
    DPC_CHECK(inserted, "attempted to push task {} more than once", task->name);
  }
  task->setStatus(Task::Submitted);
  for (auto &worker : workers) worker->push(task);
}

void NoopBackend::notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status) {
  if (status == Task::Running) {
    task->setStatus(Task::Running);
    return;
  }

  std::lock_guard<std::mutex> lock(tasks_mutex);
  auto it = tasks.find(task->id);
  if (it == tasks.end()) return;

  // Promote to worse status if this worker reported worse
  if (status > it->second.worst) it->second.worst = status;

  if (--it->second.remaining == 0) {
    task->setStatus(it->second.worst);
    tasks.erase(it);
  }
}

void NoopBackend::print(bool details) const { DPC_INFO("BCK: {}, workers={}", name(), conf.threads); }

// Worker::Worker(uint16_t tid, NoopBackend &backend) : tid(tid), backend(backend), thread(&Worker::loop, this){};

// Task::Status Worker::execute(std::shared_ptr<Task> task) {
//   DPC_TRACE("{}-t{}: Running task {}", backend.name(), tid, task->name);
//   std::this_thread::sleep_for(std::chrono::milliseconds(backend.conf.op_ms));
//   return Task::Completed;
// };

// // void Worker::start() {
// //   std::call_once(start_flag, [this] {
// //     running = true;
// //     cv.notify_one();
// //   });
// // }

// // void Worker::stop() {
// //   std::call_once(stop_flag, [this] {
// //     running = false;
// //     cv.notify_one();
// //   });
// // }

// void Worker::notify() {
//   std::lock_guard<std::mutex> lock(wait_mutex);
//   cv.notify_one();
// }
// void Worker::start() { notify(); }
// void Worker::stop() { notify(); }
// void Worker::join() {
//   if (thread.joinable()) thread.join();
// }

// void Worker::push(std::shared_ptr<Task> task) {
//   queue.push(task);
//   notify();
// }

// void Worker::loop() {
//   // Phase 1: park until backend leaves Init (start() called)
//   {
//     std::unique_lock<std::mutex> lock(wait_mutex);
//     cv.wait(lock, [this] { return backend.state_ != Backend::State::Init; });
//   }

//   // Phase 2: run until backend leaves Running
//   while (backend.state_ == Backend::State::Running) {
//     std::shared_ptr<Task> task;
//     if (queue.try_pop(task)) {
//       backend.notify(tid, task, Task::Running);
//       backend.notify(tid, task, execute(task));
//       continue;
//     }
//     std::unique_lock<std::mutex> lock(wait_mutex);
//     // Wait until we actually have a task or the backend is stopped
//     cv.wait(lock, [this] { return queue.pending() > 0 || backend.state_ != Backend::State::Running; });
//   }

//   // Phase 3: drain
//   std::shared_ptr<Task> task;
//   while (queue.try_pop(task)) { backend.notify(tid, task, Task::Aborted); }

//   // while (true) {
//   //   std::shared_ptr<Task> task;
//   //   if (queue.try_pop(task)) {
//   //     backend.notify(tid, task, Task::Running);
//   //     backend.notify(tid, task, execute(task));
//   //     continue;
//   //   }

//   //   std::unique_lock<std::mutex> lock(wait_mutex);
//   //   cv.wait(lock, [this] { return queue.pending() > 0 || !running.load(); });
//   //   if (!running && queue.pending() == 0) break;
//   // }
// }

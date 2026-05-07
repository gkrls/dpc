#include "dpc/backend/noop/noop_backend.h"
#include "dpc/backend/backend.h"
#include "dpc/context.h"
#include "dpc/task.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"
#include <iostream>
#include <memory>
#include <mutex>

using namespace dpc;

using Worker = dpc::NoopBackend::Worker;

NoopBackend::NoopBackend(Context &ctx, NoopConfig const &conf) : Backend(ctx, Backend::Noop), conf(conf) {
  for (auto i = 0; i < conf.threads; ++i) workers.push_back(std::unique_ptr<Worker>(new Worker(i, *this)));
}

void NoopBackend::start() {
  std::call_once(start_flag, [this] {
    DPC_DEBUG("NoopBackend starting...");
    state = State::Running;
    for (auto &w : workers) w->start();
  });
}

void NoopBackend::stop() {
  std::call_once(stop_flag, [this] {
    DPC_DEBUG("NoopBackend stoping...");
    for (auto &w : workers) w->stop();
    for (auto &w : workers) w->join();
  });
}

bool NoopBackend::push(std::shared_ptr<Task> task) {
  start();
  DPC_ERROR_IF(task->getStatus() >= Task::Submitted, "task {} already submitted to backend", task->name);
  if (state != State::Running) return false;
  {
    std::lock_guard<std::mutex> lock(tasks_mutex);
    if (tasks.find(task->id) != tasks.end()) return false;
    tasks[task->id] = conf.threads;
  }
  task->setStatus(Task::Submitted);
  for (auto &worker : workers) worker->push(task);
  return true;
}

void NoopBackend::notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status) {
  if (status == Task::Running) {
    task->setStatus(Task::Running);
    return;
  }

  if (status == Task::Failed) { task->setStatus(Task::Failed); }

  std::lock_guard<std::mutex> lock(tasks_mutex);
  auto it = tasks.find(task->id);
  if (it->second.fetch_sub(1) == 1) {
    task->setStatus(Task::Completed);
    tasks.erase(it);
  }
}

void NoopBackend::print(bool details) const {
  DPC_INFO("BCK: {}, workers={}", name(), conf.threads);
}

Worker::Worker(uint16_t tid, NoopBackend &backend) : tid(tid), backend(backend), thread(&Worker::loop, this){};

Task::Status Worker::execute(std::shared_ptr<Task> task) {
  DPC_TRACE("{}-t{}: Running task {}", backend.name(), tid, task->name);
  std::this_thread::sleep_for(std::chrono::milliseconds(backend.conf.op_ms));
  return Task::Completed;
};

void Worker::start() {
  std::call_once(start_flag, [this] {
    running = true;
    cv.notify_one();
  });
}

void Worker::stop() {
  std::call_once(stop_flag, [this] {
    running = false;
    cv.notify_one();
  });
}

void Worker::join() {
  if (thread.joinable()) thread.join();
}

void Worker::push(std::shared_ptr<Task> task) {
  queue.push(task);
  cv.notify_one();
}

void Worker::loop() {
  {
    std::unique_lock<std::mutex> lock(wait_mutex);
    cv.wait(lock, [this] { return running.load(); });
  }

  while (true) {
    std::shared_ptr<Task> task;
    if (queue.try_pop(task)) {
      backend.notify(tid, task, Task::Running);
      backend.notify(tid, task, execute(task));
      continue;
    }

    std::unique_lock<std::mutex> lock(wait_mutex);
    cv.wait(lock, [this] { return queue.pending() > 0 || !running.load(); });
    if (!running && queue.pending() == 0) break;
  }
}

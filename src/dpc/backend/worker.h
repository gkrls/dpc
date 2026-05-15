#ifndef DPC_BACKEND_WORKER_H
#define DPC_BACKEND_WORKER_H

#include "dpc/task.h"
#include "dpc/util/error.h"
#include "dpc/util/queue.h"

#include <thread>

namespace dpc {

/// Base class for queue-driven workers.
///
/// Owns a thread, a lock-free task queue, and a lifecycle.
/// A backend only calls start() / stop() / join().
/// The worker runs whatever execute() the subclass defines.
///
/// If your worker isn't queue-driven, don't inherit from this. Write your own
class Worker {
public:
  Worker(const Worker &) = delete;
  Worker &operator=(const Worker &) = delete;
  virtual ~Worker() {
    // Abort if subclass destructor did not join()
    DPC_CHECK(!thread_.joinable(), "Worker subclass forgot to call stop(1) in its destructor");
    stop(true);
  }

  // Begin processing tasks. Idempotent.
  void start() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ != State::Init) return;
      state_ = State::Running;
    }
    cv_.notify_one();
  }

  // Halt processing. The thread drains the queue (aborting remaining tasks)
  // and exits. Idempotent. Safe to call before start() -- the thread exits
  // without processing anything.
  void stop(bool join = false) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ == State::Stopped) return;
      state_ = State::Stopped;
    }
    cv_.notify_one();
    if (join) this->join();
  }

  void join() {
    if (thread_.joinable()) thread_.join();
  }

  // Push a task. Thread-safe; safe to call before or after start().
  void push(std::shared_ptr<Task> task) {
    DPC_CHECK(state_ == State::Running, "worker-{} is not running", tid());
    queue_.push(task);
    { std::lock_guard<std::mutex> lock(mutex_); }
    cv_.notify_one();
  }

  uint16_t tid() { return tid_; }

protected:
  explicit Worker(uint16_t tid) : tid_(tid), thread_(&Worker::main, this) {}

  // Run a task. Return its terminal status.
  virtual Task::Status execute(std::shared_ptr<Task> task) = 0;

  // Optional hooks.
  virtual void on_task_abort(std::shared_ptr<Task> /*task*/) {}
  virtual void on_task_start(std::shared_ptr<Task> /*task*/) {}
  virtual void on_task_finish(std::shared_ptr<Task> /*task*/, Task::Status /*status*/) {}

protected:
  // First line of every subclass destructor.
  void shutdown() {
    stop();
    if (thread_.joinable()) thread_.join();
  }

  void main() {
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
        on_task_start(task);
        on_task_finish(task, execute(task));
        continue;
      }
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return queue_.pending() > 0 || state_ == State::Stopped; });
      // if (state_ == State::Stopped) break;
    }

    // Phase 3: drain.
    std::shared_ptr<Task> task = nullptr;
    while (queue_.try_pop(task)) on_task_abort(task);
  }

private:
  enum class State { Init = 0, Running, Stopped };

private:
  uint16_t tid_;
  std::atomic<State> state_{State::Init};
  MPSCQueue<std::shared_ptr<Task>> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::thread thread_; // keep last
};

} // namespace dpc

#endif
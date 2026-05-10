#ifndef DPC_SCHEDULER_H
#define DPC_SCHEDULER_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "dpc/context.h"

namespace dpc {

class FIFOScheduler : public Context::Scheduler {
public:
  FIFOScheduler(Context &ctx, bool use_thread = true) : ctx(ctx), use_thread(use_thread) {}

  std::string name() override { return use_thread ? "fifo-threaded" : "fifo-inline"; }

  void start() override {
    if (use_thread) {
      thread = std::thread([this] { loop(); });
    }
  }

  void stop() override {
    if (!use_thread) return;

    {
      std::lock_guard<std::mutex> lock(mutex);
      stopped = true;
    }
    cv.notify_one();
    if (thread.joinable()) thread.join();
  }

  void submit(std::shared_ptr<Task> task) override {
    if (use_thread) {
      // Background Mode: Queue the task and wake the worker
      {
        std::lock_guard<std::mutex> lk(mutex);
        queue.push(task);
      }
      cv.notify_one();
    } else {
      // Inline Mode: Execute immediately on the caller's thread
      ctx.execute(task);
    }
  }

private:
  void loop() {
    std::unique_lock<std::mutex> lock(mutex);
    while (!stopped) {
      cv.wait(lock, [this] { return !queue.empty() || stopped; });
      while (!queue.empty()) {
        auto task = queue.front();
        queue.pop();
        // Unlock while executing to allow other threads to submit tasks
        lock.unlock();
        ctx.execute(task);
        lock.lock();
      }
    }
  }

  Context &ctx;
  bool use_thread;
  bool stopped = false;

  std::thread thread;
  std::mutex mutex;
  std::condition_variable cv;
  std::queue<std::shared_ptr<Task>> queue;
};

} // namespace dpc

#endif // !DPC_SCHEDULER_H
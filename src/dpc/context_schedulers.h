#ifndef DPC_SCHEDULER_H
#define DPC_SCHEDULER_H

#include <queue>
#include <thread>

#include "dpc/context.h"

namespace dpc {

class FIFOScheduler : public Context::Scheduler {
public:
  FIFOScheduler(Context &ctx) : ctx(ctx) {}

  std::string name() override { return "fifo"; }

  void start() override {
    thread = std::thread([this] { loop(); });
  }

  void stop() override {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopped = true;
    }
    cv.notify_one();
    if (thread.joinable()) thread.join();
  }

  void submit(std::shared_ptr<Task> task) override {
    {
      std::lock_guard<std::mutex> lk(mutex);
      queue.push(task);
    }
    cv.notify_one();
  }

private:
  void loop() {
    std::unique_lock<std::mutex> lock(mutex);
    while (!stopped) {
      cv.wait(lock, [this] { return !queue.empty() || stopped; });
      while (!queue.empty()) {
        auto task = queue.front();
        queue.pop();
        lock.unlock();
        ctx.execute(task);
        lock.lock();
      }
    }
  }

  Context &ctx;
  std::thread thread;
  std::mutex mutex;
  std::condition_variable cv;
  std::queue<std::shared_ptr<Task>> queue;
  bool stopped = false;
};

} // namespace dpc

#endif // !DPC_SCHEDULER_H
#ifndef DPC_CONTEXT_H
#define DPC_CONTEXT_H

#include "dpc/backend/backend.h"
#include "dpc/device.h"
#include "dpc/task.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace dpc {

class Backend;

class Context final {
public:
  enum State { Init = 1, Running, Stopping, Stopped };
  friend class FIFOScheduler;
  friend class Task;

  /// Default operation timeout (ms)
  inline static const uint32_t kDefaultOperationTimeout = 30000;

  Context(uint16_t rank, uint16_t world, uint32_t timeout = kDefaultOperationTimeout);
  // Device given — backend from env or default
  Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, uint32_t timeout = kDefaultOperationTimeout);
  // Backend given — device from env/default
  Context(uint16_t rank, uint16_t world, BackendConfig const &bc, uint32_t timeout = kDefaultOperationTimeout);
  // Both given — no env
  Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, BackendConfig const &bc,
          uint32_t timeout = kDefaultOperationTimeout);

  ~Context();

  State state() { return state_; }
  Device &device() { return *device_; }
  Backend &backend() { return *backend_; }

  operator uint32_t() const { return id; }

  void print();
  bool hasScheduler() { return scheduler_ != nullptr; }
  bool isRunning() { return state_ == Running; }
  bool isStopping() { return state_ == Stopping; }
  bool isStopped() { return state_ == Stopped; }

  /**
   * @brief Block until a task finishes or the timeout elapses
   * @param task the task to wait on
   * @param timeout maximum time to wait; zero (default) means wait indefinitely
   * @return The task's terminal status, or Task::Running if the timeout elapsed first
   */
  Task::Status wait(std::shared_ptr<Task> task, std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

  /**
   * @brief Block until all tracked tasks finish or the timeout elapses
   * @param timeout maximum time to wait; zero (default) means wait indefinitely
   * @return The number of tasks that reached a terminal status before the timeout elapsed
   */
  int waitAll(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

public:
  std::shared_ptr<Task> ReduceAsync(void const *sendbuf, void *recvbuf, uint64_t count, uint32_t root, DataType type,
                                    ReduceOp op = ReduceOp::Sum, CollectiveOptions const &opt = {});
  std::shared_ptr<Task> ReduceScatterAsync(void const *sendbuf, void *recvbuf, uint64_t recvcount, DataType type,
                                           ReduceOp op = ReduceOp::Sum, CollectiveOptions const &opt = {});
  std::shared_ptr<Task> AllReduceAsync(void const *sendbuf, void *recvbuf, uint64_t count, DataType type,
                                       ReduceOp op = ReduceOp::Sum, CollectiveOptions const &opt = {});
  std::shared_ptr<Task> AllGatherAsync(void const *sendbuf, void *recvbuf, uint64_t sendcount, DataType type,
                                       CollectiveOptions const &opt = {});

  Task::Status Reduce(void const *sendbuf, void *recvbuf, uint64_t count, uint32_t root, DataType type,
                      ReduceOp op = ReduceOp::Sum, CollectiveOptions const &opt = {});
  Task::Status ReduceScatter(void const *sendbuf, void *recvbuf, uint64_t recvcount, DataType type,
                             ReduceOp op = ReduceOp::Sum, CollectiveOptions const &opt = {});
  Task::Status AllReduce(void const *sendbuf, void *recvbuf, uint64_t count, DataType type, ReduceOp op = ReduceOp::Sum,
                         CollectiveOptions const &opt = {});
  Task::Status AllGather(void const *sendbuf, void *recvbuf, uint64_t sendcount, DataType type,
                         CollectiveOptions const &opt = {});

public:
  const uint16_t rank = 0;
  const uint16_t world = 1;
  const uint32_t id = 0;

public:
  class Scheduler {
  public:
    virtual ~Scheduler() = default;
    virtual std::string name() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void submit(std::shared_ptr<Task> task) = 0;
  };

private:
  void start();
  void stop();
  void watchdog();

  std::shared_ptr<Task> submit(std::shared_ptr<Task> t);
  /// Release a task from the context's tracking set.
  ///
  /// Called by Task::setStatus() when the task reaches a terminal status
  /// (Completed / Aborted / Failed). Removes the task from tracking_tasks and
  /// increments the appropriate lifetime counter based on its final status.
  ///
  /// Idempotent: releasing a task that is no longer tracked is a no-op.
  void release(Task &t);
  void execute(std::shared_ptr<Task> t);

private:
  bool use_scheduler_ = false;
  std::string name_;
  std::atomic<Context::State> state_{Context::Init};
  std::condition_variable state_cv;
  std::once_flag init_flag;
  std::once_flag fini_flag;
  std::mutex state_mutex;

  std::unique_ptr<Device> device_ = nullptr;
  std::unique_ptr<Backend> backend_ = nullptr;
  std::unique_ptr<Scheduler> scheduler_ = nullptr;

  // Task tracking
  std::atomic<uint64_t> submitted_{0};
  std::atomic<uint64_t> completed_{0};
  std::atomic<uint64_t> aborted_{0};
  std::atomic<uint64_t> failed_{0};
  std::atomic<uint64_t> rejected_{0};
  std::mutex tracking_mutex;
  std::unordered_map<uint64_t, std::shared_ptr<Task>> tracking_tasks;

  std::chrono::milliseconds timeout{0};
  std::atomic<pid_t> watchdog_thread_id{0};
  std::thread watchdog_thread;
};

// scheduler stuff

} // namespace dpc
#endif

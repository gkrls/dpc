#ifndef DPC_CONTEXT_H
#define DPC_CONTEXT_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#include "dpc/backend.h"
#include "dpc/collectives.h"
#include "dpc/device.h"
#include "dpc/scheduler.h"
#include "dpc/types.h"

namespace dpc {

class Backend;
class SocketBackend;

class Context final {
public:
  enum State { CREATED = 1, INITIALIZED, FINALIZING, FINALIZED };
  friend class Backend;
  friend class SocketBackend;
  friend class Task;

  Context(uint16_t rank, uint16_t world, DeviceOptions const &de = {}, BackendOptions const &be = {},
          std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});
  ~Context();

  State state();
  Device &device();
  Backend &backend();

  operator uint32_t() const { return id; }

  void print();
  bool usesScheduler();
  bool isInitialized() { return state_ == INITIALIZED; }
  bool isFinalized() { return state_ == FINALIZED; }

  Task::Status wait(std::shared_ptr<Task> task);
  Task::Status wait(std::shared_ptr<Task> task, std::chrono::milliseconds timeout);
  int waitAll();

public:
  // reduce: in and out and count elements, out only at root
  std::shared_ptr<Task> ReduceAsync(void *out, void *in, uint32_t count, DataType type, uint32_t root,
                                    ReduceOptions const &opt = {});
  // reduce_scatter: in has count * nranks elements, out has count elements
  std::shared_ptr<Task> ReduceScatterAsync(void *out, void *in, uint32_t count, DataType type,
                                           ReduceScatterOptions const &opt = {});
  // allreduce: in and out and count elements
  std::shared_ptr<Task> AllReduceAsync(void *out, void *in, uint32_t count, DataType type,
                                       AllReduceOptions const &opt = {});
  // allgather: in has count elements, out has count * world elements
  std::shared_ptr<Task> AllGatherAsync(void *in, void *out, uint32_t count, DataType type, AllGatherOptions const &opt);

  Task::Status Reduce(void *out, void *in, uint32_t count, DataType type, ReduceOp op, uint32_t root,
                      ReduceOptions const &opt = {});
  Task::Status ReduceScatter(void *out, void *in, uint32_t count, DataType type, ReduceScatterOptions const &opt = {});
  Task::Status AllReduce(void *out, void *in, uint32_t count, DataType type, AllReduceOptions const &opt = {});
  Task::Status AllGather(void *in, void *out, uint32_t count, DataType type, AllGatherOptions const &opt);

public:
  const uint16_t rank = 0;
  const uint16_t world = 1;
  const uint32_t id = 0;

private:
  /// The scheduler loop of this context, running on its own thread
  /// Ideally we should make the context accept a scheduler object, but this is
  /// fine for now.
  void start();
  void stop();
  void watchdog();
  /// Called internally to submit a task to the scheduler
  void schedule(std::shared_ptr<Task> t);

private:
  bool use_scheduler_ = false;
  std::string name_;
  std::atomic<Context::State> state_;
  std::condition_variable state_cv;
  std::once_flag init_flag;
  std::once_flag fini_flag;
  std::mutex state_mutex;

  std::shared_ptr<Device> device_ = nullptr;
  std::shared_ptr<Backend> backend_ = nullptr;

  std::shared_ptr<Scheduler> scheduler = nullptr;
  std::thread watchdog_thread;
  std::chrono::milliseconds timeout;

  std::mutex tracking_mutex;
  std::unordered_map<uint64_t, std::shared_ptr<Task>> tracking_tasks;
};
} // namespace dpc

#define DPC_FATAL(fstr, ...)                                                                                           \
  do {                                                                                                                 \
    fmt::println(stderr, "FATAL {}:{}: " fstr, __FILE_NAME__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                    \
    std::abort();                                                                                                      \
  } while (0)

#endif
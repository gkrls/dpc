#ifndef DPC_TASK_H
#define DPC_TASK_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <variant>

#include "dpc/collectives.h"

namespace dpc {

class Context;

class Task {
public:
  using id_t = uint64_t;
  using Options = std::variant<AllReduceOptions, AllGatherOptions, ReduceScatterOptions>;

  enum Status : int8_t {
    Created = -4,
    Submitted = -3,
    Running = -2,
    Aborted = -1,
    Completed = 0,
    Failed = 1,
    DidNotRun = 2,
  };

  struct Stats {
    struct {
      std::chrono::steady_clock::time_point create;
      std::chrono::steady_clock::time_point submit;
      std::chrono::steady_clock::time_point start;
      std::chrono::steady_clock::time_point finish;
    } time;
    struct {
      std::atomic<int> threads{0};
      std::unordered_map<uint16_t, float> throughput_per_thread;
    } perf;
  };

  friend class Context;

public:
  // identity & target
  Context &ctx;
  const id_t id;
  const std::string name;
  const bool async;

  // buffers
  void *const in;
  void *const out;
  const uint64_t in_count;  // elements in input buffer
  const uint64_t out_count; // elements in output buffer
  const DataType type;

  // operation
  const Collective coll;
  const ReduceOp reduce; // ignored when coll == AllGather
  const Options opt;

  // bookkeeping
  Stats stats;

public:
  // factory
  static std::shared_ptr<Task> Create(Context &ctx, Collective coll, ReduceOp reduce, void *in, void *out,
                                      uint64_t in_count, uint64_t out_count, DataType type, bool async, Options opt);

  // For AllReduce: needs reduce op, in_count == out_count
  static std::shared_ptr<Task> CreateAllReduce(Context &ctx, ReduceOp reduce, void *in, void *out, uint64_t count,
                                               DataType type, bool async, AllReduceOptions opt);

  // For AllGather: no reduce op, out_count = in_count * world
  static std::shared_ptr<Task> CreateAllGather(Context &ctx, void *in, void *out, uint64_t in_count, DataType type,
                                               bool async, AllGatherOptions opt);

  // For ReduceScatter: needs reduce op, in_count = out_count * world
  static std::shared_ptr<Task> CreateReduceScatter(Context &ctx, ReduceOp reduce, void *in, void *out,
                                                   uint64_t out_count, DataType type, bool async,
                                                   ReduceScatterOptions opt);

  Task() = delete;
  Task(Task const &) = delete;
  Task(Task &&) = delete;
  Task &operator=(Task const &) = delete;
  Task &operator=(Task &&) = delete;
  ~Task() = default;

  // lifecycle
  Status abort();
  Status wait();
  Status wait(std::chrono::milliseconds timeout);

  // status queries
  bool isRunning() const;
  bool isFinished() const;
  bool isCompleted() const;
  bool isFailed() const;
  bool isAborted() const;

  // type queries
  bool isFloatingPoint() const;
  bool isInteger() const;
  bool isSigned() const;
  bool isUnsigned() const;

  // op queries
  bool isReduction() const { return coll != Collective::AllGather; }
  bool isAllReduce() const { return coll == Collective::AllReduce; }
  bool isAllGather() const { return coll == Collective::AllGather; }
  bool isReduceScatter() const { return coll == Collective::ReduceScatter; }

  // option access — typed
  template <class T> const T &options() const { return std::get<T>(opt); }

  // accessors
  Status getStatus() const { return status; }
  std::string_view getStatusString() const { return getStatusString(status); }
  static std::string_view getStatusString(Status s);
  Context &getContext() { return ctx; }
  void *getInput() { return in; }
  void *getOutput() { return out; }

  // callbacks
  bool setCallbacks(std::function<void(Task &)> on_complete, std::function<void(Task &)> on_error = nullptr,
                    std::function<void(Task &)> on_abort = nullptr);
  bool setCompletionCallback(std::function<void(Task &)> cb);
  bool setAbortCallback(std::function<void(Task &)> cb);
  bool setErrorCallback(std::function<void(Task &)> cb);
  void clearCallbacks() {
    on_complete_cb = {};
    on_abort_cb = {};
    on_error_cb = {};
  }

  // stats
  std::unordered_map<std::string, float> getStats();
  static void printStats(std::vector<std::shared_ptr<Task>> const &tasks);

private:
  Task(Context &ctx, Collective coll, ReduceOp reduce, void *in, void *out, uint64_t in_count, uint64_t out_count,
       DataType type, bool async, Options opt);

  Status setStatus(Status s);

  std::atomic<Status> status;
  std::mutex statusMutex;
  std::condition_variable statusCv;
  std::function<void(Task &)> on_complete_cb;
  std::function<void(Task &)> on_abort_cb;
  std::function<void(Task &)> on_error_cb;
};

} // namespace dpc

#endif // DPA_TASK_H
#ifndef DPC_TASK_H
#define DPC_TASK_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>


namespace dpc {

class Context;
class Task;

enum DataType   : uint8_t { I32, U32, F32 };
enum Collective : uint8_t { AllReduce, AllGather, ReduceScatter };
enum ReduceOp   : uint8_t { Sum = 0, Min, Max, Avg, Prod, __default__ = Sum};

struct CollectiveOptions {
  int quantization = 0;
  int pipes = 0;
  std::function<void(Task&)> on_complete = nullptr;
  std::function<void(Task&)> on_abort = nullptr;
  std::function<void(Task&)> on_error = nullptr;
};

constexpr uint8_t dtypeWidth(DataType) noexcept { return 4; }


class Task {
public:
  using id_t = uint64_t;

  enum Status : int8_t {
    Created = -3,
    Submitted = -2,
    Running = -1,
    Completed = 0,
    Aborted = 1,
    Failed = 2,
  };

  struct Stats {
    struct {
      std::chrono::steady_clock::time_point create;
      std::chrono::steady_clock::time_point submit;
      std::chrono::steady_clock::time_point start;
      std::chrono::steady_clock::time_point finish;
    } time;
    // struct {
    //   std::atomic<int> threads{0};
    // } perf;
  };

  // --- identity ---
  Context &ctx;
  const id_t id;
  const std::string collname;
  const std::string name;
  const bool async;

  // --- buffers ---
  const void *const sendbuf;
  void *const recvbuf;
  const uint64_t sendcount;
  const uint64_t recvcount;
  const DataType type;

  // --- operation ---
  const Collective coll;
  const ReduceOp reduce;
  const CollectiveOptions opt;

  // --- bookkeeping ---
  Stats stats;

  // --- lifecycle ---
  bool abort();
  bool setStatus(Status s);
  Status wait();
  Status wait(std::chrono::milliseconds timeout);

  // --- status queries ---
  bool isRunning() const;
  bool isFinished() const;
  bool isCompleted() const;
  bool isFailed() const;
  bool isAborted() const;

  // --- type queries ---
  bool isFloatingPoint() const;
  bool isInteger() const;
  bool isSigned() const;
  bool isUnsigned() const;

  // --- collective queries ---
  bool isReduction() const { return coll != Collective::AllGather; }
  bool isAllReduce() const { return coll == Collective::AllReduce; }
  bool isAllGather() const { return coll == Collective::AllGather; }
  bool isReduceScatter() const { return coll == Collective::ReduceScatter; }
  bool isQuantized() const { return opt.quantization > 0; }

  // --- accessors --- 
  std::string const &toString() const;
  Status getStatus() const { return status; }
  std::string_view getStatusString() const;
  static std::string_view getStatusString(Status s);

  // --- callbacks ---
  bool setCallbacks(std::function<void(Task &)> on_complete, std::function<void(Task &)> on_error = nullptr,
                    std::function<void(Task &)> on_abort = nullptr);
  bool setCompletionCallback(std::function<void(Task &)> cb);
  bool setAbortCallback(std::function<void(Task &)> cb);
  bool setErrorCallback(std::function<void(Task &)> cb);
  void clearCallbacks();

  // --- stats ---
  std::unordered_map<std::string, float> getStats();
  static void printStats(std::vector<std::shared_ptr<Task>> const &tasks);

  // --- remove all public constructors ---
public:
  friend class Context;
  Task() = delete;
  Task(Task const &) = delete;
  Task(Task &&) = delete;
  Task &operator=(Task const &) = delete;
  Task &operator=(Task &&) = delete;
  ~Task() = default;

  // --- only context creates with factories ---
protected:
  static std::shared_ptr<Task> CreateAllReduce(Context &ctx, bool async, const void *sendbuf, void *recvbuf,
                                               uint64_t count, DataType type, ReduceOp op = ReduceOp::__default__,
                                               CollectiveOptions opt = {});

  static std::shared_ptr<Task> CreateReduceScatter(Context &ctx, bool async, const void *sendbuf, void *recvbuf,
                                                   uint64_t recvcount, DataType type, ReduceOp op = ReduceOp::__default__,
                                                   CollectiveOptions opt = {});

  static std::shared_ptr<Task> CreateAllGather(Context &ctx, bool async, const void *sendbuf, void *recvbuf,
                                               uint64_t sendcount, DataType type, CollectiveOptions opt = {});

private:
  Task(Context &ctx, bool async, const void *sendbuf, void *recvbuf, uint64_t sendcount, uint64_t recvcount,
       DataType type, ReduceOp reduce, Collective coll, CollectiveOptions opt);

  std::shared_ptr<Task> submit(std::function<std::shared_ptr<Task>()> create);

  mutable std::string str;
  std::atomic<Status> status{Status::Created};
  std::mutex statusMutex;
  std::condition_variable statusCv;
  std::function<void(Task &)> on_complete_cb;
  std::function<void(Task &)> on_abort_cb;
  std::function<void(Task &)> on_error_cb;
};

} // namespace dpc

#endif // DPC_TASK_H
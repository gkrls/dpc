#include "dpc/task.h"
#include "dpc/context.h"
#include "dpc/types.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"

#include <atomic>
#include <chrono>
#include <mutex>

using namespace dpc;

static uint64_t nextTaskID() {
  static std::atomic<uint64_t> count_(1);
  return count_.fetch_add(1);
}

static std::string collectiveName(Collective coll) {
  switch (coll) {
  case Collective::AllReduce: return "allreduce";
  case Collective::AllGather: return "allgather";
  case Collective::ReduceScatter: return "reducescatter";
  default: DPC_UNREACHABLE();
  }
}

static std::string reduceOpName(ReduceOp reduce) {
  switch (reduce) {
  case ReduceOp::Sum: return "sum";
  case ReduceOp::Min: return "min";
  case ReduceOp::Max: return "max";
  case ReduceOp::Avg: return "avg";
  case ReduceOp::Prod: return "prod";
  default: DPC_UNREACHABLE();
  }
}

// --- construction ---

Task::Task(Context &ctx, Collective coll, ReduceOp reduce, void *in, void *out, uint64_t in_count, uint64_t out_count,
           DataType type, bool async, CollectiveOptions opt)
    : id(nextTaskID()), collname(collectiveName(coll)), name(collname + " " + std::to_string(id)), ctx(ctx),
      async(async), in(in), out(out), in_count(in_count), out_count(out_count), type(type), coll(coll), reduce(reduce),
      opt(opt), stats{} {
  stats.time.create = std::chrono::steady_clock::now();
}

// --- factory methods ---

std::shared_ptr<Task> Task::CreateAllReduce(Context &ctx, ReduceOp reduce, void *in, void *out, uint64_t count,
                                            DataType type, bool async, CollectiveOptions opt) {
  DPC_ERROR_IF(!in || !out, "null buffer");
  DPC_ERROR_IF(count == 0, "zero element count");
  DPC_ERROR_IF(reduce != ReduceOp::Sum && reduce != ReduceOp::Avg, "only SUM and AVG reductions currently supported");

  if (type == DataType::I32 || type == DataType::U32) {
    DPC_ERROR_IF(opt.quantization > 0, "quantization not supported for integer types");
    DPC_ERROR_IF(reduce == ReduceOp::Avg, "averaging int input changes output type and is not currently supported");
  } else {
    DPC_ERROR_IF(opt.quantization == 0, "float allreduce requires quantization");
    DPC_ERROR_IF(opt.quantization > ctx.device().conf.exponents, "{}-block quantization not supported by device",
                 opt.quantization);
  }

  if (!opt.pipes) opt.pipes = ctx.device().conf.pipes;

  DPC_ERROR_IF(opt.pipes > ctx.device().conf.pipes, "requested pipes ({}) > device pipes ({})", opt.pipes,
               ctx.device().conf.pipes);

  return std::shared_ptr<Task>(new Task(ctx, Collective::AllReduce, reduce, in, out, count, count, type, async, opt));
}

std::shared_ptr<Task> Task::CreateReduceScatter(Context &ctx, ReduceOp reduce, void *in, void *out, uint64_t out_count,
                                                DataType type, bool async, CollectiveOptions opt) {
  DPC_ERROR_IF(!in || !out, "null buffer");
  DPC_ERROR_IF(out_count == 0, "zero element count");
  DPC_ERROR_IF(reduce != ReduceOp::Sum && reduce != ReduceOp::Avg, "only SUM and AVG reductions currently supported");

  if (type == DataType::I32 || type == DataType::U32) {
    DPC_ERROR_IF(opt.quantization > 0, "quantization not supported for integer types");
    DPC_ERROR_IF(reduce == ReduceOp::Avg, "averaging int input changes output type and is not currently supported");
  } else {
    DPC_ERROR_IF(opt.quantization == 0, "float reduce-scatter requires quantization");
    DPC_ERROR_IF(opt.quantization > ctx.device().conf.exponents, "{}-block quantization not supported by device",
                 opt.quantization);
  }

  if (!opt.pipes) opt.pipes = ctx.device().conf.pipes;

  DPC_ERROR_IF(opt.pipes > ctx.device().conf.pipes, "requested pipes ({}) > device pipes ({})", opt.pipes,
               ctx.device().conf.pipes);

  uint64_t in_count = out_count * ctx.world;

  return std::shared_ptr<Task>(
      new Task(ctx, Collective::ReduceScatter, reduce, in, out, in_count, out_count, type, async, opt));
}

std::shared_ptr<Task> Task::CreateAllGather(Context &ctx, void *in, void *out, uint64_t in_count, DataType type,
                                            bool async, CollectiveOptions opt) {
  DPC_ERROR_IF(!in || !out, "null buffer");
  DPC_ERROR_IF(in_count == 0, "zero element count");
  DPC_ERROR_IF(opt.quantization > 0, "quantization not supported for allgather");
  DPC_ERROR_IF(opt.pipes > 0, "pipes not supported for allgather");

  uint64_t out_count = in_count * ctx.world;

  return std::shared_ptr<Task>(
      new Task(ctx, Collective::AllGather, ReduceOp::Sum, in, out, in_count, out_count, type, async, opt));
}

// --- lifecycle ---

Task::Status Task::abort() { return setStatus(Status::Aborted); }

Task::Status Task::wait() {
  std::unique_lock<std::mutex> lock(statusMutex);
  statusCv.wait(lock, [this] { return isFinished(); });
  return status;
}

Task::Status Task::wait(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(statusMutex);
  statusCv.wait_for(lock, timeout, [this] { return isFinished(); });
  return status;
}

// --- status ---

std::string_view Task::getStatusString(Task::Status s) {
  switch (s) {
  case Task::Created: return "Created";
  case Task::Submitted: return "Submitted";
  case Task::Running: return "Running";
  case Task::Aborted: return "Aborted";
  case Task::Completed: return "Completed";
  case Task::Failed: return "Failed";
  case Task::DidNotRun: return "DidNotRun";
  }
  return "Unknown";
}

std::string_view Task::getStatusString() const { return getStatusString(status); }

bool Task::isRunning() const { return status == Status::Running; }
bool Task::isFinished() const { return status > Status::Running; }
bool Task::isCompleted() const { return status == Status::Completed; }
bool Task::isFailed() const { return status == Status::Failed; }
bool Task::isAborted() const { return status == Status::Aborted; }

bool Task::isFloatingPoint() const { return type == DataType::F32; }
bool Task::isInteger() const { return !isFloatingPoint(); }
bool Task::isSigned() const { return type != DataType::U32; }
bool Task::isUnsigned() const { return type == DataType::U32; }

Task::Status Task::setStatus(Status s) {
  std::lock_guard<std::mutex> lock(statusMutex);
  Status old = this->status;

  if (s == Status::Aborted && old > s) return old;

  DPC_ERROR_IF(static_cast<int>(s) < static_cast<int>(old), "task {}: invalid status transition '{}' -> '{}'", name,
               getStatusString(old), getStatusString(s));

  if (s <= old) return old;

  if (s == Status::Running) stats.time.start = std::chrono::steady_clock::now();
  this->status = s;

  if (isFinished()) {
    if (s == Status::Completed) stats.time.finish = std::chrono::steady_clock::now();
    statusCv.notify_all();

    if (isAborted() && on_abort_cb) on_abort_cb(*this);
    else if (isCompleted() && on_complete_cb) on_complete_cb(*this);
    else if (isFailed() && on_error_cb) on_error_cb(*this);
  }

  return old;
}

// --- callbacks ---

bool Task::setCallbacks(std::function<void(Task &)> on_complete, std::function<void(Task &)> on_error,
                        std::function<void(Task &)> on_abort) {
  std::lock_guard<std::mutex> lock(statusMutex);
  if (isFinished()) return false;
  on_complete_cb = std::move(on_complete);
  on_error_cb = std::move(on_error);
  on_abort_cb = std::move(on_abort);
  return true;
}

bool Task::setCompletionCallback(std::function<void(Task &)> cb) {
  std::lock_guard<std::mutex> lock(statusMutex);
  if (isFinished()) return false;
  on_complete_cb = std::move(cb);
  return true;
}

bool Task::setAbortCallback(std::function<void(Task &)> cb) {
  std::lock_guard<std::mutex> lock(statusMutex);
  if (isFinished()) return false;
  on_abort_cb = std::move(cb);
  return true;
}

bool Task::setErrorCallback(std::function<void(Task &)> cb) {
  std::lock_guard<std::mutex> lock(statusMutex);
  if (isFinished()) return false;
  on_error_cb = std::move(cb);
  return true;
}

void Task::clearCallbacks() {
  std::lock_guard<std::mutex> lock(statusMutex);
  on_complete_cb = {};
  on_abort_cb = {};
  on_error_cb = {};
}

// --- stats ---

std::unordered_map<std::string, float> Task::getStats() {
  std::unordered_map<std::string, float> out;

  float time_ms = 0.0f;
  if (stats.time.start.time_since_epoch().count() && stats.time.finish.time_since_epoch().count() &&
      stats.time.finish >= stats.time.start) {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(stats.time.finish - stats.time.start).count();
    time_ms = static_cast<float>(us / 1000.0);
  }

  float time_s = time_ms > 0.0f ? time_ms / 1000.0f : 1e-9f;
  float elements = static_cast<float>(in_count);
  float bytes = static_cast<float>(in_count * dtypeWidth(type));

  out["time_ms"] = time_ms;
  out["threads"] = static_cast<float>(stats.perf.threads.load());
  out["elements"] = elements;
  out["bytes"] = bytes;
  out["elems_per_s"] = elements / time_s;
  out["bytes_per_s"] = bytes / time_s;

  return out;
}

void Task::printStats(std::vector<std::shared_ptr<Task>> const &tasks) {
  if (tasks.empty()) return;

  for (size_t i = 0; i < tasks.size(); ++i) {
    auto s = tasks[i]->getStats();
    DPC_INFO("task {:>3}  {:>8.2f} ms  {:>10.0f} elems  {:>12.2f} elems/s  {:>12.2f} B/s", i, s["time_ms"],
             s["elements"], s["elems_per_s"], s["bytes_per_s"]);
  }
}
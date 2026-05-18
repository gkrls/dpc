#include "dpc/context.h"

#include "dpc/backend/backend.h"
#include "dpc/backend/noop/noop_backend.h"
#if DPC_DPDK_ENABLED
#include "dpc/backend/dpdk/dpdk_backend.h"
#endif
#include "dpc/config.h"
#include "dpc/context_schedulers.h"
#include "dpc/device.h"
#include "dpc/task.h"
#include "dpc/util/env.h"
#include "dpc/util/error.h"
#include "dpc/util/log.h"

#include "fmt/core.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <string_view>
#include <thread>
#include <unistd.h>

using namespace dpc;

#include <string>

namespace {

static uint64_t getUniqueID() {
  static std::atomic<uint64_t> count_(0);
  return count_.fetch_add(1);
}

const auto kScheduler = env::getstr({"DPC_SCHEDULER"}, {"off", "on", "fifo", "fifo-threaded"}).value_or("off");
const auto kTimeout = env::getuint({"DPC_TIMEOUT"});

DeviceConfig resolveDevice() {
  if (auto path = env::getstr({"DPC_DEVICE"})) return DeviceConfig::fromJson(std::string{*path});
  return DeviceConfig::GenericTofino1;
}

// Resolve backend from env (config file + kind), falling back to default-constructed.
std::unique_ptr<BackendConfig> resolveBackend() {
  auto config_path = env::getstr({"DPC_CONFIG"});
  auto backend_name = env::getstr({"DPC_BACKEND"});

  if (config_path && backend_name) {
    auto kind = Backend::get(std::string{*backend_name});
    return BackendConfig::fromJson(std::string{*config_path}, kind);
  }

  if (config_path) {
    // file given but no backend selector — use first one found
    return BackendConfig::fromJson(std::string{*config_path});
  }

  if (backend_name) {
    // kind given but no file — default-construct that kind
    switch (Backend::get(std::string{*backend_name})) {
    case Backend::Noop: return std::make_unique<NoopConfig>();
    case Backend::Dpdk: return std::make_unique<DpdkConfig>();
    }
    DPC_ERROR("unhandled backend kind");
  }

  // neither given — default Noop with defaults
  return std::make_unique<NoopConfig>();
}

} // namespace

std::unique_ptr<Context::Scheduler> create_scheduler(Context &ctx, const std::string_view sched) {
  const std::string kDefaultScheduler = "fifo-threaded";
  if (sched == "off") return nullptr;
  else if (sched == "on") return std::make_unique<FIFOScheduler>(ctx, false);
  else if (sched == "fifo") return std::make_unique<FIFOScheduler>(ctx, false);
  else if (sched == "fifo-threaded") return std::make_unique<FIFOScheduler>(ctx, true);
  DPC_UNREACHABLE("bad scheduler name");
  return nullptr;
}

Context::Context(uint16_t rank, uint16_t world, uint32_t timeout)
    : Context(rank, world, resolveDevice(), *resolveBackend(), timeout) {}
Context::Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, uint32_t timeout)
    : Context(rank, world, dc, *resolveBackend(), timeout) {}

// Context::Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, std::string be, uint32_t timeout)
//     : Context(rank, world, dc, Backend::get(be), timeout) {}

// Context::Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, Backend::Kind kind, uint32_t timeout)
//     : rank(rank), world(world), id(getUniqueID()), name_(std::string("ctx-") + std::to_string(id)),
//       state_(Context::Init), timeout(timeout) {
//   DPC_FATAL_IF(id > 0, "multiple contexts not supported yet");

//   this->device_ = std::make_unique<Device>(dc);
//   this->backend_ = Backend::create(*this, kind);
//   DPC_FATAL_IF(!this->backend_, "failed to create backend '{}'", Backend::getName(kind)); // options().name);

//   this->scheduler_ = kScheduler != "off" ? create_scheduler(*this, kScheduler) : nullptr;
//   this->timeout = std::chrono::milliseconds(kTimeout.has_value() ? *kTimeout : Context::kDefaultOperationTimeout);

//   // this->scheduler = std::make_unique<FIFOScheduler>(*this);
//   // if (DPC_TIMEOUT) this->timeout = std::chrono::milliseconds(*DPC_TIMEOUT);

//   // PrintContextInfo(*this);
//   print();
//   start();
// }

static std::atomic<int> live_contexts{0};


Context::Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, BackendConfig const &bc, uint32_t timeout)
    : rank(rank), world(world), id(getUniqueID()), name_(std::string("ctx-") + std::to_string(id)),
      state_(Context::Init), timeout(timeout) {
  DPC_FATAL_IF(live_contexts.fetch_add(1) > 0, "multiple contexts not supported yet");

  this->device_ = std::make_unique<Device>(dc);
  this->backend_ = Backend::create(*this, bc);
  this->scheduler_ = kScheduler != "off" ? create_scheduler(*this, kScheduler) : nullptr;
  this->timeout = std::chrono::milliseconds(kTimeout.value_or(timeout));

  DPC_ERROR_IF(!this->backend_, "failed to create backend '{}'", Backend::name(bc.kind_)); // options().name);
  // if (kScheduler.has_value()) this->scheduler_ = create_scheduler(*this, *kScheduler);
  // if (kTimeout.has_value()) this->timeout = std::chrono::milliseconds(*kTimeout);
  // PrintContextInfo(*this);
  print();
  start();
}

Context::~Context() { stop(); live_contexts.fetch_sub(1); }

void Context::print() {
  // DPC_INFO("{}", std::string(100, '='));
  DPC_INFO("CTX: rank={} world={} scheduler={} watchdog={} (build {} {})", rank, world, hasScheduler() ? "on" : "off",
           this->timeout.count() ? fmt::format("{}ms", this->timeout.count()) : "off", DPC_BUILD_TYPE,
           DPC_AVX512_AVAILABLE ? "avx512"
           : DPC_AVX2_AVAILABLE ? "avx2"
                                : "no-simd");
  device().print(true);
  backend().print(true);
  // DPC_INFO("{}", std::string(100, '='));
}

void Context::start() {
  std::call_once(init_flag, [this] {
    DPC_DEBUG("{} starting backend {}", name_, backend_->name());

    backend().start();

    if (scheduler_) {
      DPC_DEBUG("{} starting scheduler {}", name_, scheduler_->name());
      scheduler_->start();
    }

    if (timeout.count()) {
      watchdog_thread = std::thread(&Context::watchdog, this);
      while (watchdog_thread_id.load(std::memory_order_acquire) == 0) std::this_thread::yield();
      DPC_DEBUG("{} starting watchdog thread {}", name_, watchdog_thread_id.load());
    }

    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::Running;
      state_cv.notify_all();
    }
  });
}

void Context::stop() {
  std::call_once(fini_flag, [this] {
    DPC_DEBUG("ctx-{} stopping by thread {}", id, gettid());

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // Make sure no new tasks are accepted
    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::Stopping;
      // state_cv.notify_one();
    }

    // Snapshot what's outstanding before we tear anything down
    size_t running = 0, queued = 0;
    {
      std::lock_guard<std::mutex> lock(tracking_mutex);
      for (auto &[id, task] : tracking_tasks) {
        if (task->isFinished()) continue;
        if (task->isRunning()) ++running;
        else if (!task->isFinished()) ++queued;
      }
    }

    DPC_INFO("ctx-{} stopping with {} running, {} queued tasks", this->id, running, queued);

    if (scheduler_) scheduler_->stop();

    backend().stop();

    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::Stopped;
      state_cv.notify_all();
    }

    if (watchdog_thread.joinable()) watchdog_thread.join();

    // {
    //   std::lock_guard<std::mutex> lock(tracking_mutex);
    //   tracking_tasks.clear();
    // }

    // DPC_INFO("ctx-{} killed by thread {}", id, gettid());
  });
}

void Context::release(Task &task) {
  // DPC_DEBUG("release: task {} status={}", task.id, task.getStatusString());
  std::lock_guard<std::mutex> lock(tracking_mutex);
  if (tracking_tasks.erase(task.id) == 0) return;
  switch (task.getStatus()) {
  case Task::Completed: completed_.fetch_add(1); break;
  case Task::Aborted: aborted_.fetch_add(1); break;
  case Task::Failed: failed_.fetch_add(1); break;
  default: break;
  }
}

std::shared_ptr<Task> Context::submit(std::shared_ptr<Task> task) {
  if (state_ != Running) {
    rejected_.fetch_add(1);
    task->setStatus(Task::Aborted);
    return task;
  }

  {
    std::lock_guard<std::mutex> lock(tracking_mutex);
    auto [it, inserted] = tracking_tasks.try_emplace(task->id, task);
    DPC_CHECK(inserted, "duplicate task id {}", task->id);
    submitted_.fetch_add(1);
  }

  DPC_DEBUG("new {}", task->toString());

  if (scheduler_) scheduler_->submit(task);
  else execute(task);

  return task;
}

void Context::execute(std::shared_ptr<Task> task) {
  DPC_CHECK(&task->ctx == this, "task {} not created by this context", task->name);
  DPC_CHECK(task->getStatus() == Task::Created, "task {} with status '{}'", task->name, task->getStatusString());
  backend_->push(task);
}

void Context::watchdog() {
  DPC_CHECK(timeout > std::chrono::milliseconds::zero(), "watchdog should not run with timeout 0");

  watchdog_thread_id.store(gettid(), std::memory_order_release);

  auto poll_interval = std::clamp(timeout / 10, std::chrono::milliseconds(10), std::chrono::milliseconds(500));
  // std::max(timeout / 10, std::chrono::milliseconds(100));

  std::unique_lock<std::mutex> state_lock(state_mutex);
  state_cv.wait(state_lock, [this] { return state_ >= Context::Running; });

  while (state_ < Context::Stopped) {
    state_cv.wait_for(state_lock, poll_interval, [this] { return state_ >= Context::Stopped; });

    if (state_ >= Context::Stopped) break;

    state_lock.unlock();
    {
      std::lock_guard<std::mutex> tracking_lock(tracking_mutex);
      auto now = std::chrono::steady_clock::now();

      for (auto &[id, task] : tracking_tasks) {
        if (task->isRunning()) {
          auto duration = now - task->stats.time.start;
          if (duration > timeout) {
            DPC_FATAL("watchdog: task {} did not finish within {} ms", task->name,
                      std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), timeout.count());
            // DPC_FATAL("watchdog: task {} timed out ({}ms > {}ms)", task->name,
            //           std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), timeout.count());
          }
        }
      }
    }

    // {
    //   std::lock_guard<std::mutex> tracking_lock(tracking_mutex);
    //   auto now = std::chrono::steady_clock::now();
    //   // DPC_DEBUG("watchdog poll: {} tasks tracked", tracking_tasks.size());
    //   for (auto &[id, task] : tracking_tasks) {
    //     auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - task->stats.time.start).count();
    //     // DPC_DEBUG("  task {} status={} running={} elapsed={}ms", task->name, task->getStatusString(),
    //     // task->isRunning(),
    //     //           elapsed);
    //     if (task->isRunning() && (now - task->stats.time.start) > timeout) {
    //       DPC_FATAL("watchdog: task {} did not finish in {}ms", task->name,
    //                 std::chrono::duration_cast<std::chrono::milliseconds>(now - task->stats.time.start).count());
    //     }
    //   }
    // }
    state_lock.lock();
  }

  DPC_DEBUG("watchdog thread stopped");
}

std::shared_ptr<Task> Context::ReduceAsync(void const *sendbuf, void *recvbuf, uint64_t count, uint32_t root,
                                           DataType type, ReduceOp op, CollectiveOptions const &opt) {
  DPC_ERROR("Reduce collective not implemented");
}

Task::Status Context::Reduce(void const *sendbuf, void *recvbuf, uint64_t count, uint32_t root, DataType type,
                             ReduceOp op, CollectiveOptions const &opt) {
  DPC_ERROR("Reduce collective not implemented");
}

std::shared_ptr<Task> Context::ReduceScatterAsync(void const *sendbuf, void *recvbuf, uint64_t recvcount, DataType type,
                                                  ReduceOp op, CollectiveOptions const &opt) {
  return submit(Task::CreateReduceScatter(*this, true, sendbuf, recvbuf, recvcount, type, op, opt));
}

Task::Status Context::ReduceScatter(void const *sendbuf, void *recvbuf, uint64_t recvcount, DataType type, ReduceOp op,
                                    CollectiveOptions const &opt) {
  return ReduceScatterAsync(sendbuf, recvbuf, recvcount, type, op, opt)->wait();
}

std::shared_ptr<Task> Context::AllReduceAsync(void const *sendbuf, void *recvbuf, uint64_t count, DataType type,
                                              ReduceOp op, CollectiveOptions const &opt) {
  return submit(Task::CreateAllReduce(*this, true, sendbuf, recvbuf, count, type, op, opt));
}

Task::Status Context::AllReduce(void const *sendbuf, void *recvbuf, uint64_t count, DataType type, ReduceOp op,
                                CollectiveOptions const &opt) {
  return AllReduceAsync(sendbuf, recvbuf, count, type, op, opt)->wait();
}

std::shared_ptr<Task> Context::AllGatherAsync(void const *sendbuf, void *recvbuf, uint64_t sendcount, DataType type,
                                              CollectiveOptions const &opt) {

  return submit(Task::CreateAllGather(*this, true, sendbuf, recvbuf, sendcount, type, opt));
}

Task::Status Context::AllGather(void const *sendbuf, void *recvbuf, uint64_t sendcount, DataType type,
                                CollectiveOptions const &opt) {
  return AllGatherAsync(sendbuf, recvbuf, sendcount, type, opt)->wait();
}
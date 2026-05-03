#include "dpc/context.h"
#include "dpc/config.h"
#include "dpc/device.h"
#include "dpc/types.h"
#include "dpc/util/env.h"
#include "dpc/util/error.h"
#include <cstdio>
#include <memory>
#include <thread>
#include <unistd.h>

#include "dpc/backend/backend.h"
#include "dpc/context_schedulers.h"

using namespace dpc;

#include <string>

namespace {

static uint64_t getUniqueID() {
  static std::atomic<uint64_t> count_(0);
  return count_.fetch_add(1);
}

DPC_ENV_BOOL(DPA_SCHEDULER, "DPA_SCHEDULER");
DPC_ENV_UINT(DPA_TIMEOUT, "DPA_TIMEOUT");

} // namespace

Context::Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, uint32_t timeout)
    : Context(rank, world, dc, "noop", timeout) {}
Context::Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, std::string be, uint32_t timeout)
    : Context(rank, world, dc, Backend::get(be), timeout) {}

Context::Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, Backend::Kind kind, uint32_t timeout)
    : rank(rank), world(world), id(getUniqueID()), name_(std::string("ctx-") + std::to_string(id)),
      state_(Context::Created), timeout(timeout) {
  DPC_FATAL_IF(id > 0, "multiple contexts not supported yet");

  this->device_ = std::make_shared<Device>(dc);
  this->backend_ = Backend::create(*this, kind);
  DPC_FATAL_IF(!this->backend_, "failed to create backend '{}'", Backend::getName(kind)); // options().name);

  if (DPA_SCHEDULER.value_or(false)) this->scheduler = std::make_unique<FIFOScheduler>(*this);
  if (DPA_TIMEOUT) this->timeout = std::chrono::milliseconds(*DPA_TIMEOUT);

  // PrintContextInfo(*this);
  print();
  start();
}

Context::Context(uint16_t rank, uint16_t world, DeviceConfig const &dc, BackendConfig const &bc, uint32_t timeout)
    : rank(rank), world(world), id(getUniqueID()), name_(std::string("ctx-") + std::to_string(id)),
      state_(Context::Created), timeout(timeout) {
  DPC_FATAL_IF(id > 0, "multiple contexts not supported yet");

  this->device_ = std::make_shared<Device>(dc);
  this->backend_ = Backend::create(*this, bc);
  DPC_FATAL_IF(!this->backend_, "failed to create backend '{}'", bc.getBackendName()); // options().name);

  if (DPA_SCHEDULER.value_or(false)) this->scheduler = std::make_unique<FIFOScheduler>(*this);
  if (DPA_TIMEOUT) this->timeout = std::chrono::milliseconds(*DPA_TIMEOUT);

  // PrintContextInfo(*this);
  print();
  start();
}

Context::~Context() { stop(); }

void Context::print() {
  DPC_INFO("{}", std::string(100, '='));
  DPC_INFO("CTX: rank={} world={} scheduler={} (build {} {})", rank, world, usesScheduler() ? "on" : "off",
           DPC_BUILD_TYPE,
           DPC_AVX512_AVAILABLE ? "avx512"
           : DPC_AVX2_AVAILABLE ? "avx2"
                                : "no-simd");
  device().print(true);
  backend().print(true);
  DPC_INFO("{}", std::string(100, '='));
}

void Context::start() {
  std::call_once(init_flag, [this] {
    backend().start();

    if (scheduler) scheduler->start();

    if (timeout.count()) watchdog_thread = std::thread([this] { watchdog(); });

    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::Running;
      state_cv.notify_one();
    }
  });
}

void Context::stop() {
  std::call_once(fini_flag, [this] {
    DPC_DEBUG("Context '{}' stopping...", this->name_);
    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::Finalizing;
      state_cv.notify_one();
    }

    if (scheduler) scheduler->stop();

    backend().stop();

    if (watchdog_thread.joinable()) watchdog_thread.join();

    {
      std::lock_guard<std::mutex> lock(tracking_mutex);
      tracking_tasks.clear();
    }

    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::Finalized;
      state_cv.notify_all();
    }
    DPC_INFO("ctx-{} killed by thread {}", id, gettid());
  });
}

void Context::schedule(std::shared_ptr<Task> task) {
  DPC_ERROR_IF(&task->ctx != this, "task {} not created by this context", task->name);
  DPC_ERROR_IF(task->getStatus() > Task::Created, "task {} with status '{}'", task->name, task->getStatusString());

  {
    std::lock_guard<std::mutex> lock(state_mutex);
    DPC_ERROR_IF(state_ >= Context::Finalizing, "cannot schedule task on finalizing/finalized context");
  }

  // Mark task waitable first, to avoid task finishing really fast before we can wait on it
  {
    std::lock_guard<std::mutex> lock(tracking_mutex);
    tracking_tasks[task->id] = task;
  }

  // Queue the task
  if (scheduler) {
    scheduler->submit(task);
  } else {
    execute(task);
  }
}

void Context::execute(std::shared_ptr<Task> task) {
  DPC_ERROR_IF(&task->ctx != this, "task {} not created by this context", task->name);
  DPC_ERROR_IF(task->getStatus() > Task::Created, "task {} with status '{}'", task->name, task->getStatusString());
  DPC_ERROR_IF(!backend_->push(task), "failed to push task {} to backend");
}

void Context::watchdog() {
  DPC_ERROR_IF(timeout == std::chrono::milliseconds::zero(), "watchdog should not run with timeout 0");
  DPC_DEBUG("watchdog thread started for context {}", name_);

  auto poll_interval = std::max(timeout / 10, std::chrono::milliseconds(100));

  std::unique_lock<std::mutex> state_lock(state_mutex);
  state_cv.wait(state_lock, [this] { return state_ >= Context::Running; });

  while (state_ < Context::Finalizing) {
    state_cv.wait_for(state_lock, std::chrono::milliseconds(poll_interval),
                      [this] { return state_ >= Context::Finalizing; });
    state_lock.unlock();

    {
      std::lock_guard<std::mutex> tracking_lock(tracking_mutex);
      auto now = std::chrono::steady_clock::now();
      for (auto &[id, task] : tracking_tasks) {
        if (task->isRunning() && (now - task->stats.time.start) > timeout) {
          DPC_FATAL("watchdog: task {}/{} did not finish in {}ms", task->id, task->name,
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - task->stats.time.start).count());
        }
      }
    }

    state_lock.lock();
  }

  DPC_DEBUG("watchdog thread stopped");
}

std::shared_ptr<Task> Context::AllReduceAsync(void *out, void *in, uint32_t size, DataType type, ReduceOp op,
                                              CollectiveOptions const &o) {
  printf("out: %p\n", out);
  printf("out: %p\n", in);
  printf("size: %d\n", size);
  DPC_ERROR_IF(!in || !out || !size, "Invalid task input or size");
  auto task = Task::CreateAllReduce(*this, true, in, out, size, type, op, o);
  schedule(task);
  return task;
}

Task::Status Context::AllReduce(void *out, void *in, uint32_t size, DataType type, ReduceOp op,
                                CollectiveOptions const &o) {
  return AllReduceAsync(out, in, size, type, op, o)->wait();
}

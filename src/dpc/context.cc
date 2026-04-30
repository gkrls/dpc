#include "dpc/context.h"
#include "dpc/backend.h"
#include "dpc/device.h"
#include "dpc/scheduler.h"
#include "dpc/util/env.h"
#include "dpc/util/log.h"
#include <memory>
#include <thread>
#include <unistd.h>

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

Context::Context(uint16_t rank, uint16_t world, DeviceOptions const &dev, BackendOptions const &be,
                 std::chrono::milliseconds timeout)
    : rank(rank), world(world), id(getUniqueID()), name_(std::string("ctx-") + std::to_string(id)),
      state_(Context::CREATED), timeout(timeout) {

  DPC_FATAL_IF(id > 0, "multiple contexts not supported yet");

  dpc::log::init();

  this->device_ = std::make_shared<Device>(dev);
  this->backend_ = Backend::create(*this, be);
  DPC_FATAL_IF(!this->backend_, "failed to create backend '{}'", be.getBackendName()); // options().name);

  if (DPA_SCHEDULER.value_or(false)) this->scheduler = std::make_unique<FIFOScheduler>(*backend_);
  if (DPA_TIMEOUT) this->timeout = std::chrono::milliseconds(*DPA_TIMEOUT);

  // PrintContextInfo(*this);
  print();
  start();
}

Context::~Context() { stop(); }

void Context::print() {
  DPC_INFO("", std::string(100, '='));
  DPC_INFO("Context: rank={} world={} scheduler={} build={} avx={} ", rank, world, usesScheduler() ? "on" : "off",
           "TODO", "TODO");
  device().print(true);
  backend().print(true);
  DPC_INFO("", std::string(100, '='));
}

void Context::start() {
  std::call_once(init_flag, [this] {
    backend().start();

    if (scheduler) scheduler->start();

    if (timeout.count()) watchdog_thread = std::thread([this] { watchdog(); });

    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::INITIALIZED;
      state_cv.notify_one();
    }
  });
}

void Context::stop() {
  std::call_once(fini_flag, [this] {
    DPC_DEBUG("Context '{}' stopping...", this->name_);
    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::FINALIZING;
      state_cv.notify_one();
    }

    if (scheduler) scheduler->stop();
    backend_->stop();
    if (watchdog_thread.joinable()) watchdog_thread.join();

    {
      std::lock_guard<std::mutex> lock(tracking_mutex);
      tracking_tasks.clear();
    }

    {
      std::lock_guard<std::mutex> lock(state_mutex);
      state_ = Context::FINALIZED;
      state_cv.notify_one();
    }
    DPC_INFO("ctx-{} killed by thread {}", id, gettid());
  });
}

void Context::watchdog() {
  DPC_ERROR_IF(timeout == std::chrono::milliseconds::zero(), "watchdog should not run with timeout 0");
  std::lock_guard<std::mutex> lk(tracking_mutex);
  auto now = std::chrono::steady_clock::now();
  for (auto &[id, task] : tracking_tasks) {
    if (task->isRunning() && (now - task->stats.time.start) > timeout) {
      DPC_ERROR("task {}/{} did not finish in {}ms", task->id, task->name,
                std::chrono::duration_cast<std::chrono::milliseconds>(now - task->stats.time.start).count());
    }
  }
}
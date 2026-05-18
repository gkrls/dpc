#ifndef DPC_BACKEND_H
#define DPC_BACKEND_H

#include "dpc/config.h"
#include "dpc/task.h"
#include "dpc/util/error.h"
#include "dpc/util/queue.h"

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace dpc {

class Context;
class BackendConfig;
class BackendWorker;

/**
 * @brief Base abstract class for all backends
 *
 * Only the context is meant to use it
 */
class Backend {
  friend class Context;
  friend class BackendConfig;

public:
  // ============= BACKEND REGISTRATION =============
  enum Kind {
    Noop = 0,
#if DPC_DPDK_ENABLED
    Dpdk,
#endif
  };

private:
  static inline constexpr std::pair<Backend::Kind, const char *> registry[] = {
      {Backend::Noop, "noop"},
#if DPC_DPDK_ENABLED
      {Backend::Dpdk, "dpdk"},
#endif
  };
  // ================================================

public:
  static std::string name(Backend::Kind kind);
  static Backend::Kind kind(std::string_view name);
  static Backend::Kind get(std::string_view name);

  enum State { Init = 1, Running, Stopping, Stopped };

  Backend() = delete;
  Backend(Backend &&) = delete;
  Backend(Backend const &) = delete;
  void operator=(Backend const &) = delete;
  Backend &operator=(Backend &&) = delete;

  virtual ~Backend() = default;

  virtual std::string name() const { return name_; }
  /**
   * @brief Print a summary about his backend to stdout
   */
  virtual void print(bool details) const = 0;
  /**
   * @brief Check if this backend supports a collective on a give datatype
   */
  virtual bool supports(Collective c, DataType t) const = 0;
  /**
   * @brief Retrieve the backend's configuration
   */
  virtual const BackendConfig &config() const = 0;
  /**
   * @brief Fetch the context of this Backend
   */
  virtual Context &context() const { return ctx_; }

  /**
   * @brief Check if a backend is a certain kind
   */
  bool is(Backend::Kind kind) const { return kind_ == kind; }
  /**
   * @brief Retrieve the backend's state
   */
  State state() const { return state_.load(); }

protected:
  Backend(Context &ctx, Backend::Kind kind) : ctx_(ctx), kind_(kind), name_(name(kind)) {}

  /**
   * @brief Start the backend, creating all necessary resources
   * After this call the backend is ready to execute tasks
   */
  virtual void start() = 0;
  /**
   * @brief Stop the backend, destroying resources etc
   * After this call the backend cannot accept tasks
   */
  virtual void stop() = 0;
  /**
   * @brief Submit a task to the backend for execution
   * This is a non-blocking call that should return immediatelly
   * Task status is handled by the Task object itself
   */
  virtual void push(std::shared_ptr<Task> task) = 0;
  /**
   * Create a backend instance
   */
  static std::unique_ptr<Backend> create(Context &ctx, BackendConfig const &conf);
  static std::unique_ptr<Backend> create(Context &ctx, Kind kind);

  Context &ctx_;

  const Backend::Kind kind_;

  const std::string name_;

  std::atomic<State> state_{State::Init};
};

/**
 * @brief Base class for configuration options for each backend
 *
 * Holds no actual configuration options besides the backend kind
 */
class BackendConfig {

  friend class Context;
  friend class Backend;

public:
  BackendConfig() = delete;
  BackendConfig(BackendConfig &&) = default;
  BackendConfig(BackendConfig const &) = default;
  BackendConfig(Backend::Kind kind) : kind_(kind) {}
  BackendConfig &operator=(BackendConfig const &) = delete; // assignment: drop
  BackendConfig &operator=(BackendConfig &&) = delete;
  virtual ~BackendConfig() = default;

  /**
   * @brief Create a backend config from the first one found in the json config. Throw if none
   */
  static std::unique_ptr<BackendConfig> fromJson(const std::string &path);
  /**
   * @brief Create a config for Backend @p kind from the json config @p path. Throw if not found
   */
  static std::unique_ptr<BackendConfig> fromJson(const std::string &path, Backend::Kind kind);

public:
  template <typename T> T *as() {
    static_assert(std::is_base_of_v<BackendConfig, T>, "T must derive from BackendConfig");
    return dynamic_cast<T *>(this);
  }
  template <typename T> const T *as() const {
    static_assert(std::is_base_of_v<BackendConfig, T>, "T must derive from BackendConfig");
    return dynamic_cast<const T *>(this);
  }
  template <typename T> bool is() const {
    static_assert(std::is_base_of_v<BackendConfig, T>, "T must derive from BackendConfig");
    return dynamic_cast<const T *>(this) != nullptr;
  }
  bool is(Backend::Kind kind) const { return kind_ == kind; }

protected:
  const Backend::Kind kind_;
};

/**
 * @brief Base class for queue-driven backend workers.
 * Owns a thread, a lock-free task queue, and lifecycle.
 * A backend impl. calls start() / stop() / join() and push()
 * The worker runs whatever execute() the subclass defines.
 */
class BackendWorker {
public:
  BackendWorker(const BackendWorker &) = delete;
  BackendWorker &operator=(const BackendWorker &) = delete;

  virtual ~BackendWorker() {
    // Abort if subclass destructor did not join()
    DPC_CHECK(!thread_.joinable(), "Worker subclass forgot to call stop(1) in its destructor");
    stop(true);
  }

  /**
   * @brief Start polling the queue and submitting tasks
   * A worker is only started once. This call is idempotent.
   */
  void start() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ != State::Init) return;
      state_ = State::Running;
    }
    cv_.notify_one();
  }

  /**
   * @brief Stop the worker and optionally wait for its thread to finish
   *
   * A worker is only stopped once. This is call idempotent.
   * If stop is called before start, the worker cannot start afterwards
   *
   * @param join if true wait the workers's thread
   */
  void stop(bool join = false) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (state_ == State::Stopped) return;
      state_ = State::Stopped;
    }
    cv_.notify_one();
    if (join) this->join();
  }

  // Push a task. Thread-safe; safe to call before or after start().

  /**
   * @brief Push a task to the worker's queue. Thread safe
   *
   * @param task the task
   */
  void push(std::shared_ptr<Task> task) {
    DPC_CHECK(state_ == State::Running, "worker-{} is not running", id_);
    queue_.push(task);
    { std::lock_guard<std::mutex> lock(mutex_); }
    cv_.notify_one();
  }

  void join() { this->thread_.join(); }

  uint16_t id() const { return id_; }

protected:
  explicit BackendWorker(uint16_t id) : id_(id), thread_(&BackendWorker::main, this) {}

  virtual Task::Status execute(std::shared_ptr<Task> task) = 0;

  // Optional hooks.
  virtual void on_task_abort(std::shared_ptr<Task> /*task*/) {}
  virtual void on_task_start(std::shared_ptr<Task> /*task*/) {}
  virtual void on_task_finish(std::shared_ptr<Task> /*task*/, Task::Status /*status*/) {}

  void main() {
    // Phase 1: park until start() or stop().
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return state_ != State::Init; });
      // if (state_ == State::Stopped) return; // stop() before start()
    }

    // Phase 2: process tasks until stop().
    while (true) {
      if (state_ == State::Stopped) break; // check first cause
      std::shared_ptr<Task> task;
      if (queue_.try_pop(task)) {
        on_task_start(task);
        on_task_finish(task, execute(task));
        continue;
      }
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return queue_.pending() > 0 || state_ == State::Stopped; });
      // if (state_ == State::Stopped) break;
    }

    // Phase 3: drain.
    std::shared_ptr<Task> task = nullptr;
    while (queue_.try_pop(task)) on_task_abort(task);
  }

private:
  enum class State { Init = 0, Running, Stopped };

  uint16_t id_;
  std::atomic<State> state_{State::Init};
  MPSCQueue<std::shared_ptr<Task>> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::thread thread_;
};

} // namespace dpc
#endif

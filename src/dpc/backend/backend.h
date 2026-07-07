#ifndef DPC_BACKEND_H
#define DPC_BACKEND_H

#include "dpc/config.h"
#include "dpc/task.h"
#include "dpc/util/queue.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace dpc {

// class Context;
class BackendConfig;
// class BackendWorker;

/**
 * @brief Base abstract class for all backends
 *
 * Only the context is meant to use it
 */
class Backend {
public:
  // ============= BACKEND REGISTRATION =============
  enum Kind {
    Noop = 0,
    Sock,
#if DPC_DPDK_ENABLED
    Dpdk,
#endif

    Default = Sock,
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
   * @brief Retrieve the backend's configuration
   */
  virtual const BackendConfig &config() const = 0;
  /**
   * @brief Fetch the context of this Backend
   */
  virtual Context &context() const { return ctx_; }

  /**
   * @brief Check if this backend supports a collective on a give datatype
   */
  virtual bool supports(Collective c, DataType t) const { return true; }

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
  virtual void start() { /* empty */
    ;
  }
  /**
   * @brief Stop the backend, destroying resources etc
   * After this call the backend cannot accept tasks
   */
  virtual void stop(){/* empty */};
  /**
   * @brief Submit a task to the backend for execution
   * This is a non-blocking call that should return immediatelly
   * Task status is handled by the Task object itself
   */
  virtual void push(std::shared_ptr<Task> task) = 0;
  /**
   * Create a backend instance
   */
  static std::unique_ptr<Backend> create(Context &ctx, const BackendConfig &conf);
  static std::unique_ptr<Backend> create(Context &ctx, Kind kind);

  Context &ctx_;

  const Backend::Kind kind_;

  const std::string name_;

  std::atomic<State> state_{State::Init};

private:
  template <typename B, typename C>
  static std::unique_ptr<Backend> make_backend(Context &ctx, const BackendConfig &conf) {
    static_assert(std::is_base_of_v<Backend, B>);
    static_assert(std::is_base_of_v<BackendConfig, C>);
    return std::unique_ptr<B>(new B(ctx, static_cast<const C &>(conf)));
  }
  template <typename C> static std::unique_ptr<BackendConfig> make_config(const std::string &path) {
    static_assert(std::is_base_of_v<BackendConfig, C>);
    return path.empty() ? std::make_unique<C>() : std::make_unique<C>(C::fromJson(path));
  }
  struct Entry {
    Kind kind;
    const char *name;
    std::unique_ptr<Backend> (*make_backend)(Context &, const BackendConfig &);
    std::unique_ptr<BackendConfig> (*make_config)(const std::string &);
  };
  static const std::vector<Entry> registry;

  friend class BackendConfig;
  friend class Context;
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
  BackendConfig(Backend::Kind kind) : backend_name(Backend::name(kind)), kind_(kind) {}
  BackendConfig &operator=(BackendConfig const &) = delete;
  BackendConfig &operator=(BackendConfig &&) = delete;
  virtual ~BackendConfig() = default;

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

  /**
   * @brief Create a backend config from the first one found in the json config. Throw if none
   */
  static std::unique_ptr<BackendConfig> fromJson(const std::string &path);
  /**
   * @brief Create a config for Backend @p kind from the json config @p path. Throw if not found
   */
  static std::unique_ptr<BackendConfig> fromJson(const std::string &path, Backend::Kind kind);
  static std::unique_ptr<BackendConfig> fromJson(const std::string &path, std::string &name);
  /**
   * @brief Create a default config for backend @p kind
   */
  static std::unique_ptr<BackendConfig> get(Backend::Kind kind);
  /**
   * @brief Create a default config for backend @p name
   */
  static std::unique_ptr<BackendConfig> get(const std::string &name);

  const std::string backend_name;

protected:
  const Backend::Kind kind_;
};

class MultiworkerBackend;

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
    // DPC_CHECK(!thread_.joinable(), "Worker subclass forgot to call stop(1) in its destructor");
    stop(true);
  }

  /**
   * @brief Get the worker's id (within the backend)
   */
  uint16_t id() const { return id_; }

  /**
   * @brief Start polling the queue and submitting tasks
   * A worker is only started once. This call is idempotent.
   */
  void start();

  /**
   * @brief Stop the worker and optionally wait for its thread to finish
   *
   * A worker is only stopped once. This is call idempotent.
   * If stop is called before start, the worker cannot start afterwards
   *
   * @param join if true wait the workers's thread
   */
  void stop(bool join = false);

  /**
   * @brief Push a task to the worker's queue. Thread safe
   *
   * @param task the task
   */
  void push(std::shared_ptr<Task> task);

  /**
   * @brief Block until the worker's thread finishes
   */
  virtual void join() = 0;

  // Optional hooks.
  virtual void on_task_abort(std::shared_ptr<Task> /*task*/) {}
  virtual void on_task_start(std::shared_ptr<Task> /*task*/) {}
  virtual void on_task_finish(std::shared_ptr<Task> /*task*/, Task::Status /*status*/) {}

protected:
  explicit BackendWorker(MultiworkerBackend &backend, uint16_t id) : id_(id), backend_(backend) {}
  virtual Task::Status execute(std::shared_ptr<Task> task) = 0;
  virtual void main();
  uint16_t id_;
  MultiworkerBackend &backend_;

private:
  enum class State { Init = 0, Running, Stopped };
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<State> state_{State::Init};
  MPSCQueue<std::shared_ptr<Task>> queue_;
};

/**
 * @brief Base class for Backends delegating work to @BackendWorker threads
 * Owns backend workers, keeps track of task state and tracks worker task completions
 */
class MultiworkerBackend : public Backend {
  friend class BackendWorker;

public:
  MultiworkerBackend() = delete;
  MultiworkerBackend(Context &ctx, Backend::Kind kind) : Backend(ctx, kind) {}
  virtual ~MultiworkerBackend() { stop(); }

  virtual void start() override;
  virtual void stop() override;
  virtual void push(std::shared_ptr<Task> task) override;

protected:
  virtual void notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status);
  // Let subclasses populate this with whatever BackendWorker class they use
  std::vector<std::unique_ptr<BackendWorker>> workers;

private:
  struct TaskProgress {
    uint16_t remaining_workers = 1;
    Task::Status worst = Task::Completed;
  };
  std::once_flag start_flag;
  std::once_flag stop_flag;
  std::mutex state_mutex;
  std::mutex work_mutex;
  std::unordered_map<Task::id_t, TaskProgress> work;
};

} // namespace dpc
#endif

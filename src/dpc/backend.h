#ifndef DPC_BACKEND_H
#define DPC_BACKEND_H

#include <string>

#include "dpc/task.h"
#include "dpc/types.h"

namespace dpc {

///
/// The available DPA backens
/// Each backend should have an entry on this enum
///

///
/// Base class for all backend options
/// Each backend should provide its own options class that extends this
///
enum class BackendKind { Socket, Dpdk };

class Context;

class BackendOptions {

  friend class Context;
  friend class Backend;

protected:

  BackendOptions() = default;
  BackendOptions(BackendKind kind) : kind(kind) {} // name(name) {}
  BackendOptions(BackendOptions &&) = default;
  BackendOptions(BackendOptions const &) = default;

public:
  virtual ~BackendOptions() = default;
  virtual BackendOptions &operator=(const BackendOptions &) = default;
  virtual std::string str() const { return "unknown-backend-options"; }
  virtual std::string getBackendName() const { return "unknown-backend"; };

public:
  template <typename T> bool is() const { return dynamic_cast<const T *>(this) != nullptr; }
  template <typename T> T *as() const { return dynamic_cast<T *>(this); }
  template <typename T> const T *as() const { return dynamic_cast<const T *>(this); }

private:
  BackendKind kind;
};

///
/// Base abstract class for all backend implementations
/// Only the Context is meant to create and access it
///
class Backend {
  friend class Context;
public:
  enum State { Created = 1, Initialized, Finalizing, Finalized };

  Backend() = delete;
  Backend(Backend &&) = delete;
  Backend(Backend const &) = delete;
  void operator=(Backend const &) = delete;
  Backend &operator=(Backend &&) = delete;

public:
  Backend(Context &ctx, BackendOptions &o);

  virtual std::string name() const = 0;
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
  virtual bool push(std::shared_ptr<Task> task) = 0;
  /**
   * @brief Print a summary about his backend to stdout
   */
  virtual void print(bool details) const = 0;
  /**
   * @brief Check if this backend supports a collective
   */
  virtual bool supports(Collective c);
  /**
   * @brief Check if this backend supports a collective on a give datatype
   */
  virtual bool supports(Collective c, DataType t) const = 0;
  /**
   * @brief Check if this backend supports a collective
   */
  virtual const BackendOptions& options() const = 0;
  /**
   * @brief Fetch teh context of this Backend
   */
  virtual Context &context() const { return ctx; }

protected:
  /**
   * Create a backend instance from BackendOptions
   * If opts is a subclass of BackendOptions the apropriate backend is created and returned
   * If not, nullptr is returned, signifying an error
   */
  static std::shared_ptr<Backend> create(Context &ctx, BackendOptions const &opts);
  /**
   * Retrieve a backend's name
   */
  static std::string getName(BackendOptions opts) { return getName(opts.kind); }
  static std::string getName(BackendKind kind);

private:
  Context &ctx;
};


///
/// Base class for backend worker threads
/// Each backend should extend this class
///
// class BackendWorker {
// public:
//   const uint16_t tid;
// };

} // namespace dpc
#endif
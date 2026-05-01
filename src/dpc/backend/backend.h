#ifndef DPC_BACKEND_H
#define DPC_BACKEND_H

#include <string>
#include <unordered_map>

#include "dpc/task.h"
#include "dpc/types.h"
#include "dpc/util/error.h"

namespace dpc {

class Context;
class BackendConfig;

///
/// Base abstract class for all backend implementations
/// Only the Context is meant to create and access it
///
class Backend {
  friend class Context;

public:
  // BACKEND REGISTRATION
  enum Kind { Null, Dpdk };
  static inline constexpr std::pair<Kind, const char*> registry[] = {
      {Null, "null"},
      {Dpdk, "dpdk"},
  };
  static inline std::string getName(Kind kind) {
    for (auto& [k, n] : registry)
      if (k == kind) return n;
    DPC_FATAL("internal: unregistered backend kind");
  }

  static inline Kind getKind(std::string_view name) {
    for (auto& [k, n] : registry)
      if (n == name) return k;
    DPC_FATAL("internal: unregistered backend name");
  }

  // static std::string getName(Kind kind) {
  //   static const std::unordered_map<Kind, std::string> names = {{Null, "null"}, {Dpdk, "dpdk"}};
  //   auto it = names.find(kind);
  //   if (it == names.end())
  //     DPC_FATAL("internal: backend name is not registered for this backend kind");
  //   return it->second;
  // }
  // static std::string getName(BackendConfig const& conf);

  enum State { Created = 1, Initialized, Finalizing, Finalized };

  Backend() = delete;
  Backend(Backend &&) = delete;
  Backend(Backend const &) = delete;
  void operator=(Backend const &) = delete;
  Backend &operator=(Backend &&) = delete;

public:
  Backend(Context &ctx, Backend::Kind kind) : ctx(ctx), kind_(kind), name_(getName(kind)) {}

  virtual std::string name() { return name_; }
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
  virtual bool supports(Collective c) const = 0;
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
  virtual Context &context() const { return ctx; }


  bool is(Backend::Kind kind) const { return kind_ == kind; }

protected:
  /**
   * Create a backend instance from BackendOptions
   * If opts is a subclass of BackendOptions the apropriate backend is created and returned
   * If not, nullptr is returned, signifying an error
   */
  static std::shared_ptr<Backend> create(Context &ctx, BackendConfig const &conf);
  static std::shared_ptr<Backend> create(Context &ctx, Kind kind);

  Context &ctx;

private:
  const std::string name_;
  Backend::Kind kind_;
};

class BackendConfig {

  friend class Context;
  friend class Backend;

protected:
  BackendConfig() = delete;
  BackendConfig(Backend::Kind kind) : kind_(kind) {} // name(name) {}
  BackendConfig(BackendConfig &&) = default;
  BackendConfig(BackendConfig const &) = default;

public:
  virtual ~BackendConfig() = default;
  virtual BackendConfig &operator=(const BackendConfig &) = default;
  virtual std::string str() const { return "unknown-backend-options-string"; }
  virtual std::string getBackendName() const { return Backend::getName(kind_); };

public:
  // template <typename T> bool is() const { return dynamic_cast<const T *>(this) != nullptr; }
  // template <typename T> const T *as() const { return dynamic_cast<const T *>(this); }

  bool is(Backend::Kind kind) const { return kind_ == kind; }

protected:
  Backend::Kind kind_;
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
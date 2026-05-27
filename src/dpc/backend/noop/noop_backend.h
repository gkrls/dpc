#ifndef DPC_BACKEND_NULL
#define DPC_BACKEND_NULL

#include "dpc/backend/backend.h"
#include "dpc/context.h"
#include "dpc/util/log.h"

#include <memory>
#include <thread>

namespace dpc {

class NoopConfig : public BackendConfig {
public:
  NoopConfig() : BackendConfig(Backend::Noop) {}
  NoopConfig(uint64_t op_ms, uint16_t threads) : BackendConfig(Backend::Noop), op_ms(op_ms), threads(threads) {}
  uint64_t op_ms = 500;
  uint16_t threads = 2;
  static NoopConfig fromJson(const std::string &path);
};

class NoopBackend : public MultiworkerBackend {
  friend class Backend;    // Backend needs to be able to build
  friend class NoopWorker; // Workers need to call notify

private:
  NoopBackend(Context &ctx, NoopConfig const &conf = {});

  // virtual void start() override;
  // virtual void stop() override;
  // virtual void push(std::shared_ptr<Task> task) override;
  virtual void print(bool details) const override;

  virtual bool supports(Collective, DataType) const override { return true; };
  /**
   * @brief Check if this backend supports a collective
   */
  virtual const BackendConfig &config() const override { return conf; };

  NoopConfig conf;
};

class NoopWorker : public BackendWorker {
public:
  // friend class NoopBackend;
  NoopWorker(uint16_t tid, NoopBackend &backend)
      : BackendWorker(backend, tid), backend_(backend), conf_(backend_.conf), thread_(&NoopWorker::main, this) {}

protected:
  Task::Status execute(std::shared_ptr<Task> task) override {
    DPC_TRACE("{}-t{}: Running task {}", backend_.name(), id(), task->name);
    if (conf_.op_ms) std::this_thread::sleep_for(std::chrono::milliseconds(backend_.conf.op_ms));
    return Task::Completed;
  }

  void run() {}

  void join() override {
    if (thread_.joinable()) thread_.join();
  }

private:
  NoopBackend &backend_;
  NoopConfig conf_;
  std::thread thread_;
};

} // namespace dpc

#endif // !DPC_BACKEND_NULL

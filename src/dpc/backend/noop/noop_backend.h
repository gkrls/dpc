#ifndef DPC_BACKEND_NULL
#define DPC_BACKEND_NULL

#include "dpc/backend/backend.h"
#include "dpc/backend/worker.h"
#include "dpc/context.h"
#include "dpc/util/log.h"

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace dpc {

class NoopConfig : public BackendConfig {
public:
  NoopConfig() : BackendConfig(Backend::Noop) {}
  NoopConfig(uint64_t op_ms, uint16_t threads) : BackendConfig(Backend::Noop), op_ms(op_ms), threads(threads) {}
  uint64_t op_ms = 500;
  uint16_t threads = 2;
  static NoopConfig fromJson(const std::string &path);
};

class NoopBackend : public Backend {
  friend class Backend;
  friend class Context;

public:
  class Worker : public dpc::Worker {
  public:
    Worker(uint16_t tid, NoopBackend &backend) : dpc::Worker(tid), backend_(backend) {}

  protected:
    Task::Status execute(std::shared_ptr<Task> task) override {
      DPC_TRACE("{}-t{}: Running task {}", backend_.name(), tid(), task->name);
      std::this_thread::sleep_for(std::chrono::milliseconds(backend_.conf.op_ms));
      return Task::Completed;
    }

    void on_task_start(std::shared_ptr<Task> task) override { backend_.notify(tid(), task, Task::Running); }
    void on_task_finish(std::shared_ptr<Task> task, Task::Status status) override {
      backend_.notify(tid(), task, status);
    }
    void on_task_abort(std::shared_ptr<Task> task) override { backend_.notify(tid(), task, Task::Aborted); }

  private:
    NoopBackend &backend_;
  };

public:
  using Config = NoopConfig;
  // class Worker {
  //   friend class NoopBackend;

  // protected:
  //   Worker(uint16_t tid, NoopBackend &backend);
  //   void push(std::shared_ptr<Task> task);
  //   void start();
  //   void stop();
  //   void join();

  // private:
  //   void notify();
  //   void loop();
  //   Task::Status execute(std::shared_ptr<Task> task);
  //   NoopBackend &backend;
  //   uint16_t tid = 0;
  //   std::atomic<bool> running{false};
  //   std::mutex wait_mutex;
  //   std::condition_variable cv;
  //   std::once_flag start_flag;
  //   std::once_flag stop_flag;
  //   MPSCQueue<std::shared_ptr<Task>> queue;
  //   std::thread thread;
  // };

  ~NoopBackend() noexcept override {
    try {
      stop();
    } catch (...) {}
  }

private:
  NoopBackend(Context &ctx, NoopConfig const &conf = {});

  virtual void start() override;
  virtual void stop() override;
  virtual void push(std::shared_ptr<Task> task) override;
  virtual void print(bool details) const override;

  virtual bool supports(Collective, DataType) const override { return true; };
  /**
   * @brief Check if this backend supports a collective
   */
  virtual const BackendConfig &config() const override { return conf; };

  // Called by workers to notify when start/finish a task
  void notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status res);

private:
  void worker_loop();

  struct TaskState {
    uint16_t remaining = 1;
    Task::Status worst = Task::Completed;
  };
  // std::atomic<State> state{Backend::Init};
  NoopConfig conf;
  std::once_flag start_flag;
  std::once_flag stop_flag;
  std::mutex tasks_mutex;
  std::unordered_map<Task::id_t, TaskState> tasks;
  std::vector<std::unique_ptr<Worker>> workers;
};

} // namespace dpc

#endif // !DPC_BACKEND_NULL
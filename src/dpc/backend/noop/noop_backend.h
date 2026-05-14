#ifndef DPC_BACKEND_NULL
#define DPC_BACKEND_NULL

#include "dpc/backend/backend.h"
#include "dpc/context.h"
#include "dpc/util/queue.h"
#include <condition_variable>
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
  static NoopConfig fromJson(const std::string & path);
};

class NoopBackend : public Backend {
  friend class Backend;
  friend class Context;

public:
  using Config = NoopConfig;
  class Worker {
    friend class NoopBackend;

  protected:
    Worker(uint16_t tid, NoopBackend &backend);
    void start();
    void stop();
    void push(std::shared_ptr<Task> task);
    void join();

  private:
    void loop();
    Task::Status execute(std::shared_ptr<Task> task);
    NoopBackend &backend;
    uint16_t tid = 0;
    std::atomic<bool> running{false};
    std::mutex wait_mutex;
    std::condition_variable cv;
    std::once_flag start_flag;
    std::once_flag stop_flag;
    MPSCQueue<std::shared_ptr<Task>> queue;
    std::thread thread;
  };

  ~NoopBackend() noexcept override {
    try {
      stop();
    } catch (...) {}
  }

private:
  NoopBackend(Context &ctx, NoopConfig const &conf = {});

  virtual void start() override;
  virtual void stop() override;
  virtual bool push(std::shared_ptr<Task> task) override;
  virtual void print(bool details) const override;

  virtual bool supports(Collective c, DataType t) const override { return true; };
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
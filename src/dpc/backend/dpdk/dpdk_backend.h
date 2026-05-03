#ifndef DPC_DPDK_BACKEND_H
#define DPC_DPDK_BACKEND_H

#include "dpc/types.h"
#include "dpc/backend/backend.h"
#include "dpc/context.h"
#include "dpc/task.h"
#include "dpc/util/slot.h"
#include "dpc/backend/dpdk/dpdk_monitor.h"
// #include "dpc/util/prof.h"
// #include "backend_dpdk_monitor.h"
// #include "dpa/util/net.h"

#include <chrono>
#include <condition_variable>
// #include <rte_byteorder.h>
// #include <rte_atomic.h>
// #include <rte_eal.h>
// #include <rte_ethdev.h>
// #include <rte_ether.h>
// #include <rte_launch.h>
// #include <rte_mbuf.h>
// #include <rte_mbuf_core.h>
// #include <rte_mempool.h>
// #include <rte_timer.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dpc {
// forward declarations to avoid heavy includes in header
class Task;
class Context;
class DpdkBackend;
class DpdkWorker;

struct DpdkConfig : public BackendConfig {
public:
  friend class DpdkBackend;
  friend class DpdkWorker;
  static constexpr uint64_t DEFAULT_TIMEOUT_US = 200;
  static constexpr float DEFAULT_TIMEOUT_INIT_SCALING = 2.0;
  static constexpr uint64_t DEFAULT_RX_INTERVAL_US = 10;
  static constexpr uint64_t DEFAULT_TX_INTERVAL_US = 50;
  static constexpr uint32_t DEFAULT_RX_RING_SIZE = 1024;
  static constexpr uint32_t DEFAULT_TX_RING_SIZE = 512;
  static constexpr uint32_t DEFAULT_RX_BURST = 32;
  static constexpr uint32_t DEFAULT_TX_BURST = 32;
  static constexpr uint16_t DEFAULT_WINDOW = 32;

#if DPA_DPDK_WIN_HUGE
  #pragma message("DPA_DPDK_WIN_HUGE Enabled")
  static constexpr uint16_t MAX_WINDOW = 2048;
#elif DPA_DPDK_WIN_LARGE
  static constexpr uint16_t MAX_WINDOW = 256;
#else
  static constexpr uint16_t MAX_WINDOW = 64;
#endif

public:
  /// Interface to use for this backend
  /// If empty the it will be inferred from the addr field
  /// If both addr+iface are supplied, addr must be bound to the iface
  /// If none is supplied an error is thrown
  std::string iface = "eth0";

  /// IP address for this backend all threads will use this address
  /// If empty, the first IP from the provided interface will be used
  /// If both addr+iface are supplied, addr must be bound to the iface
  /// If none is supplied an error is thrown
  std::string addr = "";

  /// Port number for this backend. When using multiple threads this
  /// is a base port and each thread i gets is assigned port + i
  uint16_t port = 4242;

  /// Number of threads to use for this backend
  uint16_t threads = 1;
  // bool pinned = false;
  bool async = false;
  bool drain_queues = false;
  /// Each thread operates on a sliding window of slots
  /// Each slot in the window requires 2 slots from the pool
  /// This window is the maximum number of outstanding packets/per thread
  /// and window * treads equals the total maximum of outstading packets.
  /// Window should be carefully set so as to maximize throughput.
  /// If the window is set to 0, the backend will automatically set to the
  /// maximum possible given the session pool size (dev.session.pool.size)
  uint16_t window = 0;

  bool debug_trace_packet = false;
  bool debug_trace_packet_rtx = false;

  std::chrono::microseconds timeout{DEFAULT_TIMEOUT_US};
  float timeout_init_scaling = DEFAULT_TIMEOUT_INIT_SCALING;
  std::chrono::microseconds rx_interval{DEFAULT_RX_INTERVAL_US};
  std::chrono::microseconds tx_interval{DEFAULT_TX_INTERVAL_US};

  uint16_t rx_burst = 1;
  uint16_t tx_burst = 1;
  uint16_t tx_attempts = 5;
  uint32_t rx_ring_size = 0;
  uint32_t rx_pool_size = 0;
  uint32_t rx_pool_cache = 0;
  uint32_t tx_ring_size = 0;
  uint32_t tx_pool_size = 0;
  uint32_t tx_pool_cache = 0;

  // DPDK EAL arguments
  std::string eal_port = "";
  std::string eal_iface = "";
  std::vector<std::string> eal_extra_args;

  uint32_t profile_skip = 0;

private:
  std::string eal_lcores = ""; // e.g., "0-3" or "0,2,4,6"
  std::string eal_file_prefix = "dpa-dpdk";

  bool eal_virtual = false;
  bool eal_istap = false;
  bool eal_isafp = false;
  uint16_t eal_port_id = 0;
  std::vector<std::string> eal_args;
  std::vector<char *> eal_argv;

  bool hw_csum = false;

public:
  DpdkConfig() : BackendConfig(Backend::Dpdk) {}
  virtual std::string str() const override;
  uint32_t requiredSlots() const { return window * threads * 2; }
  uint32_t maxOutstandingPackets() const { return threads * window; }

  static DpdkConfig fromJson(const std::string &path);
};

class DpdkWorker;

class DpdkBackend : public Backend {
public:
  inline static const std::string Name = "dpa-dpdk";

  friend class DpdkWorker;
  using Slot = SlotAlt;
  DpdkBackend(Context &ctx, DpdkConfig const& conf = {});
  ~DpdkBackend();
  virtual DpdkConfig const &config() const override { return conf; }
  virtual void print(bool details) const override;

protected:
  virtual bool push(std::shared_ptr<Task> task) override;
  virtual void start() override;
  virtual void stop() override;
  void notify(uint16_t tid, std::shared_ptr<Task> task, Task::Status status);

  void configureDPDKPort();

private:
  Monitor monitor;

  State state;
  DpdkConfig conf;

  std::string eal_args;

  // Hold pointers to each worker's mpool
  std::vector<rte_mempool *> rx_mpools;
  std::vector<rte_flow *> flows;

  Context &context;
  std::once_flag init_flag;
  std::once_flag fini_flag;
  std::condition_variable cv;

  std::vector<std::unique_ptr<DpdkWorker>> workers;

  struct RunningTaskInfo {
    std::shared_ptr<Task> task;
    std::atomic<uint16_t> threads;
  };

  std::mutex taskMutex;
  std::unordered_map<Task::id_t, RunningTaskInfo> tasks;
};




} // namespace dpa

#endif // !DPC_DPDK_BACKEND_H
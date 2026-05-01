#ifndef DPC_DPDK_WORKER_H
#define DPC_DPDK_WORKER_H

#include <cstdint>
#include <rte_mbuf_core.h>

#include "dpc/backend/dpdk/dpdk_backend.h"
#include "dpc/proto.h"
#include "dpc/task.h"
#include "dpc/util/bitset.h"
#include "dpc/util/prof.h"
#include "dpc/util/slot.h"
#include "dpc/util/subrange.h"

namespace dpc {

/// One thread in the DPDK backend
class DpdkWorker {
public:
  using Slot = dpc::SlotAlt;
  friend class DpdkBackend;

  /// A worker's window entry. Fields ordered for some cache friendliness
  struct alignas(64) Entry {
    // first cache line
    uint64_t timeout = 0;
    uint64_t tsc = 0;
    rte_mbuf *mbuf0 = nullptr;
    // rte_mbuf *mbuf_rtx = nullptr;

    uint32_t seq[2] = {0};  // sequence number tracking
    uint16_t slot[2] = {0}; // device slot tracking

    // minimum info needed to rebuild packet for retransmission
    uint32_t offset = 0;
    uint32_t quants = 0;
    uint32_t exponents = 0; // stored exponents
    uint16_t vcount = 0;

    flags_t flags = 0;
    bool mbuf_rebuild = false;
    bool fin = 1;
    bool ver = 0; // version
    bool firstquant = false;

    // Second cache line
    bool ver_first = 0;
    bool ver_last = 0;
    uint32_t seq_last[2] = {0};
    uint16_t idx = 0;

    inline uint16_t slot_idx() const { return slot[ver]; }
    inline uint32_t slot_seq() const { return seq[ver]; }
  };
  // std::string pktString(AllReducePacketNS const &pkt, bool tx, bool hostorder, std::string const &suffix = "",
  //                       std::chrono::steady_clock::time_point const &ts = std::chrono::steady_clock::now()) const;

  // std::string pktString(AllReducePacket const &pkt, bool tx, bool hostorder, std::string const &suffix = "",
  //                       std::chrono::steady_clock::time_point const &ts = std::chrono::steady_clock::now()) const;

  uint16_t getid() const { return tid; }

protected:
  DpdkWorker(uint16_t tid, uint16_t lcore, rte_mempool *rx_mempool, DpdkConfig &backend);
  void join();
  void start();
  void stop();
  bool push(std::shared_ptr<Task> task);
  void waitReady();
  void main();

private:
  static int trampoline(void *worker_ptr);
  void shutdown();
  void notifyReady();
  void taskLoop();
  bool taskFinished() { return active_entries.empty(); }
  bool taskStart(std::shared_ptr<Task> task);
  bool taskFinish();
  bool taskFinish(uint16_t entry);

private:
  int loop_ns(std::shared_ptr<Task> task); // straggle-unaware loop
  int sendInitialBurstNS();
  bool checkTimeoutsNS(uint64_t now);

  // Send the initial burst of packets

private:
  int loop(std::shared_ptr<Task> task); // straggle-aware loop
  int sendInitialBurst();
  int checkTimeouts(uint64_t now);
  void fastestk(const Packet &q, uint32_t bitmap, uint32_t world, DpdkWorker::Entry &e);
  void enterSynBulk(const Packet &q, DpdkWorker::Entry &e);
  void enterSyn(const Packet &q, DpdkWorker::Entry &e);
  /// Exit SYN-mode for an entry e.
  /// If received seqnum is outside the range of the current task, the entry is set to its exit state and marked
  /// finished for the task Otherwise, the entry state is ready to advance
  /// @return `true` if the entry is finished, otherwise `false`
  bool exitSyn(const Packet &q, uint32_t q_seqnum, uint32_t q_offset, DpdkWorker::Entry &e);
  void skipForward(int64_t diff, Entry &e);

private:
  std::once_flag init_flag;
  std::once_flag fini_flag;

  /// Mutex for worker-wide operations
  /// e.g. state transitions, task pushes (from the backend) etc
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic<bool> running{false};

  std::mutex mutexReady;
  std::condition_variable cvReady;
  std::atomic<bool> ready{false};

  /// Tasks to be executed by this worker
  std::queue<std::shared_ptr<Task>> queue;

  /// Current task being executed
  std::shared_ptr<Task> task;

  DpdkBackend &backend;
  DpdkConfig &conf;
  uint16_t tid;
  uint16_t lcore;

  rte_ether_addr smac;
  rte_ether_addr dmac;

  // Keep these in network order
  rte_be32_t saddr;
  rte_be32_t daddr;
  rte_be16_t sport;
  rte_be16_t dport;

  Subrange chunk;
  uint16_t win_capacity;
  uint16_t win_size = 0;

  Bitset<DpdkConfig::MAX_WINDOW> active_entries;

  Entry *win = nullptr;

  /// Pool info
  const uint16_t global_pool_base;
  const uint16_t global_pool_size;
  const uint16_t subpool_base_local;
  const uint16_t subpool_base_global;
  const uint16_t subpool_size;

  /// Packet size info
  uint16_t header_size = 0;
  uint16_t header_offset = 0;
  uint16_t payload_offset = 0;
  uint16_t payload_len = 0;
  uint16_t payload_size = 0;
  uint16_t packet_size = 0;
  uint16_t frame_size = 0;

  /// DPDK buffer management
  struct rte_mempool *rx_pool = nullptr;
  struct rte_mempool *tx_pool = nullptr;
  struct rte_mbuf **rx_mbufs = nullptr;
  struct rte_mbuf **tx_mbufs = nullptr;
  struct rte_eth_dev_tx_buffer *tx_buffer = nullptr;
  struct rte_flow *flow = nullptr;

  /// Timeouts
  uint64_t re_timeout = 0;
  uint64_t re_timeout_scaled = 0;

  /// Profiling
  prof::Prof prof;
};

} // namespace dpc

#endif // !DPC_DPDK_WORKER_H
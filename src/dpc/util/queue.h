#ifndef DPC_UTIL_QUEUE_H
#define DPC_UTIL_QUEUE_H

#include <atomic>
#include <cstdint>

namespace dpc {

template <typename T>
class MPSCQueue {
  struct Node {
    T value;
    std::atomic<Node *> next{nullptr};
    Node() = default;
    Node(T v) : value(std::move(v)) {}
  };

  std::atomic<Node *> head;
  Node *tail;  // consumer-only, no atomic needed
  std::atomic<uint32_t> count{0};

public:
  MPSCQueue() {
    auto stub = new Node();
    head.store(stub, std::memory_order_relaxed);
    tail = stub;
  }

  ~MPSCQueue() {
    while (tail) {
      auto next = tail->next.load(std::memory_order_relaxed);
      delete tail;
      tail = next;
    }
  }

  MPSCQueue(const MPSCQueue &) = delete;
  MPSCQueue &operator=(const MPSCQueue &) = delete;

  // Thread-safe, lock-free. Multiple producers OK.
  void push(T value) {
    auto node = new Node(std::move(value));
    count.fetch_add(1, std::memory_order_relaxed);
    auto prev = head.exchange(node, std::memory_order_acq_rel);
    prev->next.store(node, std::memory_order_release);
  }

  // Single consumer only. Returns false if nothing available yet.
  bool try_pop(T &out) {
    auto next = tail->next.load(std::memory_order_acquire);
    if (!next) return false;
    out = std::move(next->value);
    delete tail;
    tail = next;
    count.fetch_sub(1, std::memory_order_relaxed);
    return true;
  }

  // Lower bound. If >0, at least one push is in flight or completed.
  // May briefly return >0 when try_pop would still fail (visibility window).
  uint32_t pending() const {
    return count.load(std::memory_order_relaxed);
  }
};

} // namespace dpc

#endif // !DPC_UTIL_QUEUE_H
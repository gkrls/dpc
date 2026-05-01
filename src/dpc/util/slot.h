#ifndef DPC_UTIL_SLOT_H
#define DPC_UTIL_SLOT_H

#include <cstdint>

#include "dpc/util/error.h"

namespace dpc {

struct SlotBase {
  uint16_t g;    // global slot id
  uint16_t l;    // position relative to local slot pool
  uint16_t t;     // position relative to thread's pool (2 * window)
  uint16_t w;     // position within a window (the window entry)
  uint8_t w_ver; // version of the window entry
protected:
  SlotBase() = default;
  SlotBase(uint16_t global_slot_idx, uint16_t global_pool_base, uint16_t tid, uint16_t window);
  SlotBase(uint16_t global_slot_idx, uint16_t global_pool_base, uint16_t local_pool_base, uint16_t tid, uint16_t window);
public:
  uint16_t get_for_entry(uint16_t window, uint16_t entry, bool version, uint16_t base);
};

struct SlotStrided : public SlotBase {
  uint16_t g;    // global slot id
  uint16_t l;    // position relative to local slot pool
  uint16_t t;     // position relative to thread's pool (2 * window)
  uint16_t w;     // position within a window (the window entry)
  uint8_t w_ver; // version of the window entry
public:
  SlotStrided() : g(0), l(0), t(0), w(0) {}
  SlotStrided(uint16_t global_slot_idx, uint16_t global_pool_base, uint16_t tid, uint16_t window) {
    DPC_ASSERT(global_slot_idx >= global_pool_base, "slot must be greater than poolBase");
    g = global_slot_idx;
    l = global_slot_idx - global_pool_base;
    t = l - (tid * window * 2);
    w = t < window ? t : t - window;
    // w = t / 2;
    w_ver = t % 2; //w != t;
  }
  operator uint16_t() const { return g; }
  bool operator==(const SlotStrided &other) const { return g == other.g && l == other.l && t == other.t && w == other.w; }
  bool operator!=(const SlotStrided &other) const { return !(*this == other); }
  SlotStrided &operator=(const SlotStrided &other) = default;
  std::string str() { return fmt::format("g={} l={} t={} w={} w_ver={}", g, l, t, w, w_ver); }

  static uint16_t get_for_entry(uint16_t window, uint16_t entry, bool version, uint16_t base) {
    return base + entry + (version * window);
  }
};

struct SlotAlt : public SlotBase {
  uint16_t g;    // global slot id
  uint16_t l;    // position relative to local slot pool
  uint16_t t;     // position relative to thread's pool (2 * window)
  uint16_t w;     // position within a window (the window entry)
  uint8_t w_ver; // version of the window entry
public:
  SlotAlt() : g(0), l(0), t(0), w(0) {}
  SlotAlt(uint16_t global_slot_idx, uint16_t global_pool_base, uint16_t tid, uint16_t window) {
  }
  SlotAlt(uint16_t global_slot_idx, uint16_t global_pool_base, uint16_t local_pool_base) {
    DPC_ASSERT(global_slot_idx >= global_pool_base, "slot must be greater than poolBase");
    g = global_slot_idx;
    l = global_slot_idx - global_pool_base;
    t = l - local_pool_base; //(tid * window * 2);
    // w = t < window ? t : t - window;
    w = t / 2;
    w_ver = t % 2; //w != t;
  }
  operator uint16_t() const { return g; }
  bool operator==(const SlotAlt &other) const { return g == other.g && l == other.l && t == other.t && w == other.w; }
  bool operator!=(const SlotAlt &other) const { return !(*this == other); }
  SlotAlt &operator=(const SlotAlt &other) = default;
  std::string str() { return fmt::format("g={} l={} t={} w={} w_ver={}", g, l, t, w, w_ver); }

  static uint16_t get_for_entry(uint16_t window, uint16_t entry, bool version, uint16_t base) {
    return base + entry * 2 + version;
  }
};

}

#endif
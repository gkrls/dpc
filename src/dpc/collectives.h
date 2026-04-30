#ifndef DPC_COLLECTIVES_H
#define DPC_COLLECTIVES_H

#include <cstdint>

#include "types.h"

namespace dpc {

struct ReduceOptions {
  ReduceOp op;
  uint32_t root = 0;
  /// Number of switch pipes to use for the allreduce task
  /// This is only meant for debugging purposes.
  /// For normal operation this shoud be left at 0, which
  /// will default to the maximum pipes available in the device
  /// This is handled at the Task constructor.
  uint8_t pipes = 0;
};

struct AllReduceOptions {
  uint8_t pipes = 0;
  ReduceOp op;
};

struct ReduceScatterOptions {
  uint8_t pipes = 0;
  ReduceOp op;
};

struct AllGatherOptions {
};

}

#endif
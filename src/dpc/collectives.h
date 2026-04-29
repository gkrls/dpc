#ifndef DPC_COLLECTIVES_H
#define DPC_COLLECTIVES_H

#include <cstdint>

#include "types.h"

namespace dpc {

struct BaseCollectiveOptions {
  uint32_t timeout;
};

struct ReduceOptions : public BaseCollectiveOptions {
  uint8_t pipes;
  ReduceOp op;
};

struct AllReduceOptions : public BaseCollectiveOptions {
  uint8_t pipes;
  ReduceOp op;
};

struct ReduceScatterOptions : public BaseCollectiveOptions {
  uint8_t pipes;
  ReduceOp op;
};

struct AllGatherOptions : public BaseCollectiveOptions {
};

}

#endif
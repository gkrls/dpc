#ifndef DPC_TYPES_H
#define DPC_TYPES_H

#include <cstdint>

namespace dpc {

enum class DataType   : uint8_t { I32, U32, F32 };
enum class Collective : uint8_t { AllReduce, AllGather, ReduceScatter };
enum class ReduceOp   : uint8_t { Sum = 0, Min, Max, Avg, Prod };

struct CollectiveOptions {
  int quantization = 0;
  int pipes = 0;
};

constexpr uint8_t dtypeWidth(DataType) noexcept { return 4; }

} // namespace dpc

#endif
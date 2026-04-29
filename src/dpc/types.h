#ifndef DPC_TYPES_H
#define DPC_TYPES_H

#include <cstdint>

namespace dpc {

enum class DataType   : uint8_t { I32, U32, F32 };
enum class Collective : uint8_t { Reduce = 1, AllReduce = 2, ReduceScatter, AllGather};
enum class ReduceOp   : uint8_t { Sum, Min, Max };

constexpr uint8_t dtypeWidth(DataType) noexcept { return 4; }  // for now

} // namespace dpa

#endif
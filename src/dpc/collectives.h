#ifndef DPC_COLLECTIVES_H
#define DPC_COLLECTIVES_H

#include "dpc/util/error.h"

#include <cstdint>
#include <functional>
#include <string>

namespace dpc {

class Task;

struct CollectiveOptions {
  int quantization = 0;
  int pipes = 0;
  std::function<void(Task &)> on_complete = nullptr;
  std::function<void(Task &)> on_abort    = nullptr;
  std::function<void(Task &)> on_error    = nullptr;
};

enum DataType : uint8_t { I32, U32, F32 };
enum Collective : uint8_t { AllReduce = 0, AllGather, ReduceScatter, Reduce };
enum ReduceOp : uint8_t { Sum = 0, Min, Max, Avg, Prod, __default__ = Sum };

constexpr uint8_t getDatatypeWidth(DataType) noexcept { return 4; }

inline std::string getDatatypeName(DataType dtype) {
  switch (dtype) {
  case DataType::I32: return "i32";
  case DataType::U32: return "u32";
  case DataType::F32: return "f32";
  default: DPC_UNREACHABLE();
  }
}

inline std::string getCollectiveName(Collective coll) {
  switch (coll) {
  case Collective::AllReduce: return "allreduce";
  case Collective::AllGather: return "allgather";
  case Collective::ReduceScatter: return "reducescatter";
  default: DPC_UNREACHABLE();
  }
}

inline std::string getReduceOpName(ReduceOp reduce) {
  switch (reduce) {
  case ReduceOp::Sum: return "sum";
  case ReduceOp::Min: return "min";
  case ReduceOp::Max: return "max";
  case ReduceOp::Avg: return "avg";
  case ReduceOp::Prod: return "prod";
  default: DPC_UNREACHABLE();
  }
}

} // namespace dpc

#endif

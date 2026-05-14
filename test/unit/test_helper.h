// test/test_helpers.h
#ifndef DPC_TEST_HELPERS_H
#define DPC_TEST_HELPERS_H

#include "dpc/context.h"
#include "dpc/device.h"
#include "dpc/backend/noop/noop_backend.h"

#include <memory>

namespace dpc::test {

/// Build a Context with a NoopBackend configured for fast, deterministic tests.
/// Defaults: rank=0, world=1, op_ms=0 (instant), threads=2, timeout=30s.
inline std::unique_ptr<Context> MakeContext(uint64_t op_ms = 0,
                                            uint16_t threads = 2,
                                            uint32_t timeout_ms = 30000,
                                            uint16_t rank = 0,
                                            uint16_t world = 1) {
  NoopConfig cfg(op_ms, threads);
  return std::make_unique<Context>(rank, world, DeviceConfig::GenericTofino1, cfg, timeout_ms);
}

} // namespace dpc::test

#endif
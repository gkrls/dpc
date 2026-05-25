#include "dpc/backend/noop/noop_backend.h"

#include "dpc/backend/backend.h"
#include "dpc/context.h"
#include "dpc/task.h"
#include "dpc/util/config.h"
#include "dpc/util/env.h"
#include "dpc/util/log.h"

#include <memory>

using namespace dpc;

namespace {
const auto kThreads = env::getuint({"DPC_NOOP_THREADS"});
const auto kOpMs = env::getuint({"DPC_NOOP_OP_MS"});
} // namespace

// #define R(f) conf::json_read_if_present(c.f, root, "/noop/" #f)
//   R(threads);
//   R(op_ms);
// #undef R
//
NoopConfig NoopConfig::fromJson(const std::string &path) {
  auto j = conf::json_load_at(path, "/noop");
  NoopConfig c;
  OPT(j, c, threads);
  OPT(j, c, op_ms);
  return c;
}

NoopBackend::NoopBackend(Context &ctx, NoopConfig const &conf) : MultiworkerBackend(ctx, Backend::Noop), conf(conf) {
  for (auto i = 0; i < conf.threads; ++i) workers.push_back(std::unique_ptr<NoopWorker>(new NoopWorker(i, *this)));
}

void NoopBackend::print(bool details) const {
  DPC_INFO("backend: {}, workers={} op_ms={}", name(), conf.threads, conf.op_ms);
}

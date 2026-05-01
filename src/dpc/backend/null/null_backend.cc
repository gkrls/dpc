#include "dpc/backend/null/null_backend.h"
#include "dpc/context.h"
#include <iostream>

using namespace dpc;

NullBackend::NullBackend(Context &ctx, NullConfig const& conf) : Backend(ctx, Backend::Null) {}

void NullBackend::start() {}
void NullBackend::stop() {}
bool NullBackend::push(std::shared_ptr<Task> task) {
  return true;
}
void NullBackend::print(bool details) const { std::cout << "hello from nullbackend\n"; }
#include "dpc/backend/sock/sock_backend.h"

using namespace dpc;

SockBackend::SockBackend(Context &ctx, const SockConfig &conf) : Backend(ctx, Backend::Sock) {
  // TODO
}

SockBackend::~SockBackend() {
  // TODO
}

void SockBackend::start() {
  // TODO
}

void SockBackend::stop() {
  // TODO
};

void SockBackend::push(std::shared_ptr<Task> task) {
  // TODO
}

void SockBackend::print(bool details) const {
  // TODO
}


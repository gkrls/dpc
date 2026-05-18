
#include "dpc/backend/backend.h"

namespace dpc {

class SockConfig : public BackendConfig {
public:
  const uint16_t window = 64;

};

class SockWorker : public BackendWorker {

};

class SockBackend : public Backend {

};

} // namespace dpc

#ifndef DPC_BACKEND_NULL
#define DPC_BACKEND_NULL


#include "dpc/backend/backend.h"
#include "dpc/context.h"

namespace dpc {

class NullConfig : public BackendConfig {
public:
  NullConfig() : BackendConfig(Backend::Null) {}
  uint32_t test = 42;
};

class NullBackend : public Backend {
public:
  NullBackend(Context &ctx, NullConfig const& conf);

  virtual void start() override;
  virtual void stop() override;
  virtual bool push(std::shared_ptr<Task> task) override;
  virtual void print(bool details) const override;

  virtual bool supports(Collective c) const override { return true; };
  virtual bool supports(Collective c, DataType t) const override { return true; };
  /**
   * @brief Check if this backend supports a collective
   */
  virtual const BackendConfig& config() const override { return conf; };

private:
  NullConfig conf;
};

} // namespace dpc

#endif // !DPC_BACKEND_NULL
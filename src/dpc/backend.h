#ifndef DPC_BACKEND_H
#define DPC_BACKEND_H

#include "dpc/collectives.h"

namespace dpc {

class Backend {

public:
  virtual bool supports(Collective c);
  virtual bool supports(Collective c, DataType t) const = 0;
};

}
#endif
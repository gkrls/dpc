#include "dpc/util/cpu.h"


#include <torch/python.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  // m.doc() = docs::kModule;
  static bool module_initialized = false;
  if (module_initialized) {
    return; // Module already initialized, skip
  }
  module_initialized = true;
  m.def("get_pinned_cores", &dpc::cpu::get_pinned_cores);
}

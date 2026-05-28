#include "dpc/util/cpu.h"

#include "pg_dpc.h"

#include "torch/csrc/distributed/c10d/Backend.hpp"

// namespace py = pybind11;

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  // m.doc() = docs::kModule;
  static bool module_initialized = false;
  if (module_initialized) {
    return; // Module already initialized, skip
  }
  module_initialized = true;

  py::class_<dpc::ProcessGroupDPC::Options, c10::intrusive_ptr<dpc::ProcessGroupDPC::Options>, c10d::Backend::Options>(
      m, "Options")
      .def(py::init<>());

  m.def("get_pinned_cores", &dpc::cpu::get_pinned_cores);
  m.def("get_available_cores", &dpc::cpu::get_available_cores);
  m.def("create_process_group_dpc", &dpc::create_process_group_dpc);
}

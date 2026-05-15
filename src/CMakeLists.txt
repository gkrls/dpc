if(DPC_HOST)
  # Compiler/Linker options
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_ok OUTPUT msg)

  if(ipo_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  else()
    message(WARNING "IPO/LTO not enabled: ${msg}")
  endif()

  add_compile_options(
    $<$<CONFIG:Release>:-march=native>
    $<$<CONFIG:Release>:-mtune=native>
    $<$<CONFIG:Release>:-ffunction-sections>
    $<$<CONFIG:Release>:-fdata-sections>
  )

  add_link_options(
    $<$<CONFIG:Release>:-Wl,--gc-sections>
  )

  # SIMD detection
  set(DPC_AVX512_AVAILABLE 0)
  set(DPC_AVX2_AVAILABLE 0)

  include(CheckCPUFeatures)
  include(CheckCXXCompilerFlag)

  # Check hardware/compiler support
  check_cpu_feature("avx512f" _cpu_512f)
  check_cpu_feature("avx512dq" _cpu_512dq)
  check_cpu_feature("avx512bw" _cpu_512bw)
  check_cxx_compiler_flag("-mavx512f -mavx512dq -mavx512bw" _cc_512)
  check_cpu_feature("avx2" _cpu_avx2)
  check_cxx_compiler_flag("-mavx2 -mfma" _cc_avx2)

  # Gate on user option AND hardware
  if(DPC_AVX512 AND _cpu_512f AND _cpu_512dq AND _cpu_512bw AND _cc_512)
    set(DPC_AVX512_AVAILABLE 1)
  endif()

  if(DPC_AVX2 AND _cpu_avx2 AND _cc_avx2)
    set(DPC_AVX2_AVAILABLE 1)
  endif()

  # Apply flags
  if(DPC_AVX512_AVAILABLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512dq -mavx512bw -mavx2 -mfma")
    message(STATUS "SIMD: AVX-512")
  elseif(DPC_AVX2_AVAILABLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")
    message(STATUS "SIMD: AVX2")
  else()
    message(STATUS "SIMD: none available")
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build: keeping default optimization")
  else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")
  endif()

  add_subdirectory(dpc)

  if(DPC_TORCH)
    add_subdirectory(dpc_torch)
  endif()
endif()

if(DPC_SWITCH)
  include(FindTofino)
  find_package(Tofino QUIET)

  if(Tofino_FOUND)
    message(STATUS "Tofino SDE found:")
    message(STATUS "  Tofino_SDE: ${Tofino_SDE}")
    message(STATUS "  Tofino_SDE_INSTALL: ${Tofino_SDE_INSTALL}")
    message(STATUS "  Tofino_SDE_VERSION: ${Tofino_SDE_VERSION}")
    message(STATUS "  Tofino_P4C: ${Tofino_P4C}")
    message(STATUS "  Tofino_P4I: ${Tofino_P4I}")
  else()
    message(WARNING
      "DPC_SWITCH is ON, but Tofino SDE was not found. \n"
      "  * dpc_switch will be installed but the Tofino SDE is needed to compile the P4 programs\n"
      "  * dpc_switch_controller will be built and installed but the cli controller is unusable outside a BFRT session")
  endif()

  add_subdirectory(dpc_switch)
  add_subdirectory(dpc_switch_controller)
endif()
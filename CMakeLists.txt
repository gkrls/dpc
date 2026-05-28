cmake_minimum_required(VERSION 3.14)

project(dpc VERSION "0.1.0")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Building DPC version: ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR} ${CMAKE_BUILD_TYPE}")

# User facing options
option(DPC_HOST    "Build DPA host library" ON)
option(DPC_SWITCH  "Build DPA device library" OFF)
option(DPC_DPDK    "Build the dpdk backend" ON)
option(DPC_TORCH   "Build the DPA Pytorch plugin" ON)
option(DPC_AVX2    "Enable AVX2 if available" ON)
option(DPC_AVX512  "Enable AVX512 if available" ON)
option(DPC_TEST    "Build tests" OFF)
option(DPC_ASAN    "Build with AddressSanitizer" OFF)
option(DPC_TSAN    "Build with ThreadSanitizer" OFF)
option(DPC_DOCS    "Build documentation" OFF)
option(DPC_TRACE   "Enable tracing in Debug builds" OFF)
option(DPC_PROFILE "Enable tracing in Debug builds" OFF)
option(DPC_DEVELOP "Build DPC for development" OFF)
option(DPC_CCACHE  "Use ccache to speed up rebuilds" ON)

if(DPC_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
  else()
    message(STATUS "ccache requested but not found")
  endif()
endif()

set(CMAKE_CXX_STANDARD 17) # can do 20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wno-c++20-extensions # basically just for designated initializers in C++17
  -Wnon-virtual-dtor    # base classes need virtual destructors
  -Wcast-align          # alignment violations
  -Wno-variadic-macros
  -Wshadow=local
  -Woverloaded-virtual
  -Wnull-dereference
  -Wdouble-promotion
  -Wformat=2
  -Wno-unused-parameter
)
# add_compile_options(
#   -Wall -Wextra -Wpedantic
#   -Wshadow=local        # shadowed variables
#   -Wnon-virtual-dtor    # base classes need virtual destructors
#   -Wold-style-cast      # C-style casts in C++
#   -Wcast-align          # alignment violations
#   -Wunused              # unused vars/params
#   -Woverloaded-virtual  # accidentally hiding virtual functions
#   -Wconversion          # implicit narrowing conversions
#   -Wsign-conversion     # signed/unsigned conversions
#   -Wnull-dereference    # null deref detection
#   -Wdouble-promotion    # accidental float -> double
#   -Wformat=2            # printf-style format checks
# )
#

if(DPC_WERROR)
  add_compile_options(-Werror)
endif()

if(DPC_ASAN AND DPC_TSAN)
  message(FATAL_ERROR "DPC_ASAN and DPC_TSAN are mutually exclusive; pick one")
endif()

if(DPC_ASAN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -O1)
  add_link_options(-fsanitize=address,undefined)
endif()

if(DPC_TSAN)
  add_compile_options(-fsanitize=thread -O1)
  add_link_options(-fsanitize=thread -no-pie)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_probe "${CMAKE_BINARY_DIR}/tsan_probe.c")
    file(WRITE "${_probe}" "int main(void){return 0;}\n")

    try_run(_tsan_run_result _tsan_compile_result
      "${CMAKE_BINARY_DIR}/tsan_probe_build"
      "${_probe}"
      COMPILE_DEFINITIONS -fsanitize=thread
      LINK_OPTIONS -fsanitize=thread
      RUN_OUTPUT_VARIABLE _tsan_output
    )

    if(_tsan_compile_result AND NOT _tsan_run_result EQUAL 0)
      message(FATAL_ERROR
        "\n"
        "ThreadSanitizer probe failed at runtime:\n"
        "${_tsan_output}\n"
        "\n"
        "If you see 'unexpected memory mapping', run:\n"
        "  sudo sysctl vm.mmap_rnd_bits=28\n"
        "and re-run cmake. (Resets on reboot.)\n"
      )
    endif()
  endif()
endif()

# For internal consumption
set(DPC_DPDK_ENABLED ${DPC_DPDK})
set(DPC_PROF_ENABLED ${DPC_PROFILE})
set(DPC_TRACE_ENABLED ${DPC_TRACE})

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()


add_subdirectory(src)
add_subdirectory(tools)

if (DPC_TEST)
  enable_testing()
  add_subdirectory(test)
endif()

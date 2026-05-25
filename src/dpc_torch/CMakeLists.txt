# ============================================================================
# DPC PyTorch Plugin Build
# ============================================================================
# Two targets live here:
#
# 1. torch-plugin: This target builds and installs the plugin and should run
# in a Python env where src/dpc_torch/requirements.txt are satisfied.
# CMake just delegates to pip to run setup.py, and makes sure libdpc is
# built and Gloo headers are found by setup.py
#
# 2. torch-plugin-intellisense: This is a fake target that exists only to
# populate compile_commands.json for the LSP. It does _not_ require env
# setup. If Torch headers are not found it will download them
# ============================================================================
include(FetchContent)

set(TORCH_VER "2.7.1")
set(TORCH_TAG "v${TORCH_VER}")
set(GLOO_TAG "5354032ea08eadd7fc4456477f7f7c6308818509")

file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.inc"
)

# Needed by both build paths (pip invocation + torch include-path query).
find_package(Python REQUIRED COMPONENTS Interpreter)

# ============================================================================
# Gloo headers
# ============================================================================
# The plugin currently subclasses ProcessGroupGloo, which means it does:
#
# #define USE_C10D_GLOO
# #include <torch/csrc/distributed/c10d/ProcessGroupGloo.hpp>
#
# That header is wrapped in `#ifdef USE_C10D_GLOO`. With the macro defined,
# it expands its real body and transitively #includes <gloo/...>. Torch ships
# ProcessGroupGloo.hpp in its wheel but does NOT ship the Gloo headers it
# points to. So we fetch them ourselves at the commit torch vendored.
message(STATUS "Fetching Gloo headers for plugin build...")
FetchContent_Declare(
  gloo
  GIT_REPOSITORY https://github.com/facebookincubator/gloo.git
  GIT_TAG ${GLOO_TAG}

  # GIT_SHALLOW TRUE

  # Dummy SOURCE_SUBDIR: fetch the repo without invoking add_subdirectory.
  # We only want the headers, not Gloo's CMake build.
  SOURCE_SUBDIR _no_add_subdirectory_
)
FetchContent_MakeAvailable(gloo)

# Gloo headers include <gloo/config.h>, normally produced by Gloo's own build.
# We generate it ourselves with the feature flags we need.
set(GLOO_VERSION_MAJOR 0)
set(GLOO_VERSION_MINOR 5)
set(GLOO_VERSION_PATCH 0)
set(GLOO_USE_CUDA 1)
set(GLOO_USE_NCCL 0)
set(GLOO_USE_ROCM 0)
set(GLOO_USE_RCCL 0)
set(GLOO_USE_REDIS 0)
set(GLOO_USE_MPI 0)
set(GLOO_USE_AVX 0)
set(GLOO_USE_LIBUV 0)
set(GLOO_HAVE_TRANSPORT_TCP_TLS 1)
set(GLOO_HAVE_TRANSPORT_TCP 0)
set(GLOO_HAVE_TRANSPORT_IBVERBS 0)
set(GLOO_HAVE_TRANSPORT_UV 0)

set(GLOO_CONFIG_DIR ${CMAKE_CURRENT_BINARY_DIR}/gloo-gen)
configure_file(${gloo_SOURCE_DIR}/gloo/config.h.in ${GLOO_CONFIG_DIR}/gloo/config.h @ONLY)

# Install Gloo headers to ${CMAKE_INSTALL_PREFIX}/include. setup.py reads
# from there (via DPC_INSTALL/include), and the torch-plugin target runs
# `--target install` before `pip wheel` to stage them in time.
# redis_store.h is excluded since GLOO_USE_REDIS=0.
file(GLOB_RECURSE GLOO_HEADERS "${gloo_SOURCE_DIR}/gloo/*.h")
list(FILTER GLOO_HEADERS EXCLUDE REGEX "redis_store\\.h$")

foreach(HEADER ${GLOO_HEADERS})
  file(RELATIVE_PATH HEADER_REL_PATH ${gloo_SOURCE_DIR} ${HEADER})
  get_filename_component(HEADER_DIR ${HEADER_REL_PATH} DIRECTORY)
  install(FILES ${HEADER} DESTINATION include/${HEADER_DIR})
endforeach()

install(FILES ${GLOO_CONFIG_DIR}/gloo/config.h DESTINATION include/gloo)

if(DPC_DEVELOP)
  find_package(CUDAToolkit REQUIRED)

  # Ask the pyenv's torch (if any) where its headers live, and check the version matches
  # what we expect. A mismatched torch would give clangd misleading symbols.
  execute_process(
    COMMAND ${Python_EXECUTABLE} -c
    "import torch.utils.cpp_extension; print(torch.__version__); print(';'.join(torch.utils.cpp_extension.include_paths()))"
    OUTPUT_VARIABLE TORCH_QUERY
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE TORCH_RESULT
  )
  execute_process(
    COMMAND ${Python_EXECUTABLE} -c
    "import sysconfig; print(sysconfig.get_path('include'))"
    OUTPUT_VARIABLE PYTHON_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  set(TORCH_OK FALSE)

  if(TORCH_RESULT EQUAL 0)
    string(REPLACE "\n" ";" TORCH_QUERY_LINES "${TORCH_QUERY}")
    list(GET TORCH_QUERY_LINES 0 TORCH_FOUND_VER)
    list(GET TORCH_QUERY_LINES 1 TORCH_INCLUDE_DIRS)

    # Strip the "+cu126" local tag from "2.7.1+cu126".
    string(REGEX REPLACE "\\+.*$" "" TORCH_FOUND_VER_CLEAN "${TORCH_FOUND_VER}")

    if(TORCH_FOUND_VER_CLEAN STREQUAL TORCH_VER)
      message(STATUS "Found PyTorch ${TORCH_FOUND_VER} for intellisense")
      set(TORCH_OK TRUE)
    else()
      message(WARNING
        "venv torch is ${TORCH_FOUND_VER}, expected ${TORCH_VER} -- "
        "falling back to libtorch download for intellisense")
    endif()
  endif()

  if(NOT TORCH_OK)
    FetchContent_Declare(
      libtorch
      URL "https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-${TORCH_VER}%2Bcpu.zip"
      DOWNLOAD_EXTRACT_TIMESTAMP ON
    )

    if(NOT EXISTS "${CMAKE_BINARY_DIR}/_deps/libtorch-src/include")
      message(STATUS "Downloading LibTorch ${TORCH_VER} for intellisense...")
    endif()

    FetchContent_MakeAvailable(libtorch)
    set(TORCH_INCLUDE_DIRS
      "${libtorch_SOURCE_DIR}/include"
      "${libtorch_SOURCE_DIR}/include/torch/csrc/api/include"
    )
  endif()

  message(STATUS "TORCH_INCLUDE_DIRS: ${TORCH_INCLUDE_DIRS}")
endif()

execute_process(COMMAND python -c "import torch; print(';'.join(torch.utils.cpp_extension.include_paths()))"
  OUTPUT_VARIABLE TORCH_INCLUDE_DIRS
  ERROR_VARIABLE TORCH_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE TORCH_RESULT
)

execute_process(COMMAND python -c "import sysconfig; print(sysconfig.get_path('include'))"
  OUTPUT_VARIABLE PYTHON_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE PYTHON_RESULT
)

if(DPA_DEVELOP)
  find_package(Python REQUIRED COMPONENTS Interpreter)
  find_package(CUDAToolkit REQUIRED)

  if(TORCH_RESULT EQUAL 0 AND TORCH_INCLUDE_DIRS)
    message(STATUS "Found PyTorch headers for intellisense support")
  else()
    message(WARNING "Downloading LibTorch for intellisense...")
    FetchContent_Declare(
      libtorch
      URL "https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-${TORCH_VER}%2Bcpu.zip"
      DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(libtorch)
    set(TORCH_INCLUDE_DIRS
      "${libtorch_SOURCE_DIR}/include"
      "${libtorch_SOURCE_DIR}/include/torch/csrc/api/include"
    )
  endif()

  message(STATUS "TORCH_INCLUDE_DIRS: ${TORCH_INCLUDE_DIRS}")
  add_library(dpc-torch-intellisense EXCLUDE_FROM_ALL ${PLUGIN_SOURCES})
  target_compile_features(dpc-torch-intellisense PRIVATE cxx_std_17)

  # c10/cuda config header is normally generated by torch's build; suppress.
  target_compile_definitions(dpc-torch-intellisense PRIVATE C10_CUDA_NO_CMAKE_CONFIGURE_FILE=1)
  target_include_directories(dpc-torch-intellisense PRIVATE
    ${CUDAToolkit_INCLUDE_DIRS}
    ${TORCH_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIR}
  )
  target_include_directories(dpc-torch-intellisense PUBLIC
    ${gloo_SOURCE_DIR}
    ${GLOO_CONFIG_DIR}
  )
  target_link_libraries(dpc-torch-intellisense PUBLIC dpc_ob)

  if(DPA_DPDK)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DPDK REQUIRED IMPORTED_TARGET libdpdk)

    if(NOT DPDK_FOUND)
      pkg_check_modules(DPDK REQUIRED IMPORTED_TARGET dpdk)
    endif()

    target_include_directories(dpc-torch-intellisense PRIVATE ${DPDK_INCLUDE_DIRS})
  endif()
endif()

# ============================================================================
# torch-plugin: the real build
# ============================================================================
# Run inside a venv with torch + DPDK bindings.
# 1. check-pyvenv.py: abort if venv is wrong.
# 2. cmake install: builds dpc and stages Gloo headers.
# 3. rm stale wheels: avoids pip picking the wrong one on version bumps.
# 4. pip wheel: setup.py produces the wheel.
# 5. pip install: into the active venv.
add_custom_target(torch-plugin
  COMMAND ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-pyvenv.py
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install
  COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/lib/dpc-*.whl
  COMMAND ${Python_EXECUTABLE} -m pip wheel --no-build-isolation --no-deps
  --wheel-dir ${CMAKE_BINARY_DIR}/lib
  ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${Python_EXECUTABLE} -m pip install --force-reinstall --no-deps
  ${CMAKE_BINARY_DIR}/lib/dpc-*.whl
  USES_TERMINAL
  COMMENT "Building and installing DPC PyTorch plugin"
)

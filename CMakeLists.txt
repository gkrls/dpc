cmake_minimum_required(VERSION 3.14)

project(dpc VERSION "0.1.0")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Building DPA version: ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR} ${CMAKE_BUILD_TYPE}")



option(DPC_DEVELOP "Build DPC for development" OFF)
option(DPC_HOST    "Build DPA host library" ON)
option(DPC_SWITCH  "Build DPA device library" OFF)
option(DPC_DEVELOP "Build DPA for development" OFF)
option(DPC_DPDK    "Build the dpdk backend" ON)
option(DPC_TORCH   "Build the DPA Pytorch plugin" ON)
option(DPC_AVX     "Enable vector instructions" ON)
option(DPC_TEST    "Build tests" OFF)
option(DPC_DOCS    "Build documentation" OFF)
option(DPC_TRACE   "Enable tracing in Debug builds" OFF)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
add_subdirectory(tools)

if(NOT DPC_HOST)
  message(STATUS "Configuring libdpc - skipped (DPC_HOST=OFF)")
  return()
endif()

message(STATUS "Configuring libdpc")

include(FetchContent)

# # fmt
# message(STATUS "Fetching fmt")
# FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 10.2.1)
# FetchContent_MakeAvailable(fmt)
# include_directories(${fmt_SOURCE_DIR}/include)
# file(COPY ${fmt_SOURCE_DIR}/include/fmt DESTINATION ${CMAKE_BINARY_DIR}/include)
# install(FILES $<TARGET_FILE:fmt> DESTINATION lib)
# install(DIRECTORY ${fmt_SOURCE_DIR}/include/fmt DESTINATION include)
# message(STATUS "Fetching fmt - Done")

# # nlohman json
# FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz DOWNLOAD_EXTRACT_TIMESTAMP true)
# FetchContent_MakeAvailable(json)
# include_directories(${json_SOURCE_DIR}/include)
# install(DIRECTORY ${json_SOURCE_DIR}/include/nlohmann DESTINATION include)


# Dependencies
FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 10.2.1)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz DOWNLOAD_EXTRACT_TIMESTAMP true)
FetchContent_MakeAvailable(fmt json)

# Generated headers
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_BINARY_DIR}/include/dpc/version.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"  "${CMAKE_BINARY_DIR}/include/dpc/config.h")

# Sources
set(DPC_SOURCES
  backend/null/null_backend.cc
  backend/backend.cc
  context.cc
  device.cc
  dpc.cc
  task.cc
)

# include dpdk sources if needed
if(DPC_DPDK)
  file(GLOB DPDK_SOURCES backend/dpdk/*.cc backend/dpdk/*.cpp)
  list(APPEND DPC_SOURCES ${DPDK_SOURCES})
endif()

add_library(dpc STATIC ${DPC_SOURCES})

set_target_properties(dpc PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(dpc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dpc PUBLIC fmt nlohmann_json::nlohmann_json)

# SIMD
if(DPA_AVX512_AVAILABLE)
  target_compile_options(dpc PRIVATE -mavx512f -mavx512dq -mavx512bw -mavx2 -mfma)
elseif(DPA_AVX2_AVAILABLE)
  target_compile_options(dpc PRIVATE -mavx2 -mfma)
endif()

# DPDK
if(DPC_DPDK)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(DPDK REQUIRED IMPORTED_TARGET libdpdk)

  # strip -march/-mtune so we don't override project-level arch flags
  get_target_property(_dpdk_opts PkgConfig::DPDK INTERFACE_COMPILE_OPTIONS)
  if(_dpdk_opts)
    list(FILTER _dpdk_opts EXCLUDE REGEX "^-m(arch|tune)=")
    set_target_properties(PkgConfig::DPDK PROPERTIES INTERFACE_COMPILE_OPTIONS "${_dpdk_opts}")
  endif()

  target_link_libraries(dpc PRIVATE PkgConfig::DPDK)
  # target_include_directories(dpc PRIVATE ${DPDK_INCLUDE_DIRS})

  # generate dpdk.conf for downstream consumers (torch plugin etc.)
  execute_process(COMMAND pkg-config --variable=libdir libdpdk OUTPUT_VARIABLE DPDK_PKG_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND pkg-config --variable=pmddir libdpdk OUTPUT_VARIABLE DPDK_PMDDIR     OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND pkg-config --cflags libdpdk          OUTPUT_VARIABLE DPDK_CFLAGS     OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND pkg-config --libs-only-l libdpdk     OUTPUT_VARIABLE DPDK_LIBS_L     OUTPUT_STRIP_TRAILING_WHITESPACE)

  separate_arguments(_cflags NATIVE_COMMAND "${DPDK_CFLAGS}")
  list(FILTER _cflags EXCLUDE REGEX "^-m(arch|tune)=")
  string(REPLACE ";" " " _cflags "${_cflags}")

  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/dpdk.conf"
    CONTENT "libdir=${DPDK_PKG_LIBDIR}\npmddir=${DPDK_PMDDIR}\ncflags=${_cflags}\nlibs=${DPDK_LIBS_L}\n")
  install(FILES "${CMAKE_BINARY_DIR}/dpdk.conf" DESTINATION share/dpa)
endif()

# Install
install(TARGETS dpc ARCHIVE DESTINATION lib)
install(FILES "${CMAKE_BINARY_DIR}/include/dpc/version.h" DESTINATION include/dpc)
install(FILES "${CMAKE_BINARY_DIR}/include/dpc/config.h" DESTINATION include/dpc)

# Install public headers
file(GLOB_RECURSE DPA_HEADERS "*.h")
foreach(HEADER ${DPA_HEADERS})
  file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${HEADER})
  get_filename_component(HEADER_DIR ${REL_PATH} DIRECTORY)
  install(FILES ${HEADER} DESTINATION include/dpa/${HEADER_DIR})
endforeach()


# add_library(dpc_objects OBJECT ${DPC_SOURCES})

# target_include_directories(dpc_objects
#   PUBLIC
#   $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
#   $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
#   $<INSTALL_INTERFACE:include>
# )

# add_library(dpc STATIC
#   $<TARGET_OBJECTS:dpc_objects>
# )

# target_include_directories(dpc
#   PUBLIC
#   $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
#   $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
#   $<INSTALL_INTERFACE:include>
# )
# target_link_libraries(dpc PUBLIC fmt)


if(DPC_TRACE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(dpc_objects PUBLIC DPC_ENABLE_TRACE)
  target_compile_definitions(dpc INTERFACE DPC_ENABLE_TRACE)
endif()






# --- Install

install(FILES
  "${CMAKE_BINARY_DIR}/include/dpc/version.h"
  "${CMAKE_BINARY_DIR}/include/dpc/config.h"
  DESTINATION include/dpc
)

install(TARGETS dpc
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

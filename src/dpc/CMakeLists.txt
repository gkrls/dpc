include(FetchContent)

# fmt
message(STATUS "Fetching fmt")
FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 10.2.1)
FetchContent_MakeAvailable(fmt)
include_directories(${fmt_SOURCE_DIR}/include)
file(COPY ${fmt_SOURCE_DIR}/include/fmt DESTINATION ${CMAKE_BINARY_DIR}/include)
install(FILES $<TARGET_FILE:fmt> DESTINATION lib)
install(DIRECTORY ${fmt_SOURCE_DIR}/include/fmt DESTINATION include)
message(STATUS "Fetching fmt - Done")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_BINARY_DIR}/include/dpc/version.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"  "${CMAKE_BINARY_DIR}/include/dpc/config.h")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/version.h" DESTINATION include/dpc)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/config.h" DESTINATION include/dpc)

# target_include_directories(dpc PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include")
file(GLOB_RECURSE DPC_SOURCES "*.cpp" "*.cc")
add_library(dpc_objects OBJECT ${DPC_SOURCES})

target_include_directories(dpc_objects
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)

add_library(dpc STATIC
  $<TARGET_OBJECTS:dpc_objects>
)

target_include_directories(dpc
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(dpc PUBLIC fmt)


if(DPC_TRACE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(dpc_objects PUBLIC DPC_ENABLE_TRACE)
  target_compile_definitions(dpc INTERFACE DPC_ENABLE_TRACE)
endif()

install(TARGETS dpc
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

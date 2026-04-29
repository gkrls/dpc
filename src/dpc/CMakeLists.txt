configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/include/version.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"  "${CMAKE_CURRENT_BINARY_DIR}/include/config.h")
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

install(TARGETS dpc
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

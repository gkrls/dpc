set(CURSES_NEED_NCURSES TRUE)
find_package(Curses QUIET)

if (NOT CURSES_FOUND)
  message(STATUS "ncurses not found -- skipping mock target")
  return()
endif()

add_executable(dpc_switch dpc_switch.cc)
target_include_directories(dpc_switch PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(dpc_switch PRIVATE ${CURSES_LIBRARIES} dpc)

add_executable(dpc_worker dpc_worker.cc)
target_include_directories(dpc_worker PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(dpc_worker PRIVATE dpc)

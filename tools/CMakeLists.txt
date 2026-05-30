add_executable(dpc_tool dpc_tool.cc)
target_link_libraries(dpc_tool dpc)

add_executable(dpc_switch dpc_switch.cc)
target_link_libraries(dpc_switch dpc)

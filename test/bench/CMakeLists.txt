add_executable(bench_queue bench_queue.cc)
target_link_libraries(bench_queue PRIVATE dpc)

add_executable(bench_task bench_task.cc)
target_link_libraries(bench_task PRIVATE dpc)

include(FetchContent)

FetchContent_Declare(doctest GIT_REPOSITORY https://github.com/doctest/doctest.git GIT_TAG v2.4.11)
FetchContent_MakeAvailable(doctest)

list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
include(doctest)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_dpc_api_validation test_dpc_api_validation.cc)
target_link_libraries(test_dpc_api_validation PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_api_validation TEST_PREFIX "api/validation::")

add_executable(test_dpc_backend_worker test_dpc_backend_worker.cc)
target_link_libraries(test_dpc_backend_worker PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_backend_worker TEST_PREFIX "backend/worker::")

add_executable(test_dpc_config_parsing test_dpc_config_parsing.cc)
target_link_libraries(test_dpc_config_parsing PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_config_parsing TEST_PREFIX "config::")

add_executable(test_dpc_context_lifecycle test_dpc_context_lifecycle.cc)
target_link_libraries(test_dpc_context_lifecycle PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_context_lifecycle TEST_PREFIX "context/lifecycle::")

add_executable(test_dpc_context_submit test_dpc_context_submit.cc)
target_link_libraries(test_dpc_context_submit PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_context_submit TEST_PREFIX "context/submit::")

add_executable(test_dpc_context_watchdog test_dpc_context_watchdog.cc)
target_link_libraries(test_dpc_context_watchdog PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_context_watchdog TEST_PREFIX "context/watchdog::")

add_executable(test_dpc_task_status test_dpc_task_status.cc)
target_link_libraries(test_dpc_task_status PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_task_status TEST_PREFIX "task/status::")

add_executable(test_dpc_util_bitset test_dpc_util_bitset.cc)
target_link_libraries(test_dpc_util_bitset PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_util_bitset TEST_PREFIX "util/bitset::")

add_executable(test_dpc_util_net_addr test_dpc_util_net_addr.cc)
target_link_libraries(test_dpc_util_net_addr PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_util_net_addr TEST_PREFIX "util/net_addr::")

add_executable(test_dpc_util_queue_mpsc test_dpc_util_queue_mpsc.cc)
target_link_libraries(test_dpc_util_queue_mpsc PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_util_queue_mpsc TEST_PREFIX "util/queue::")

add_executable(test_dpc_util_subrange test_dpc_util_subrange.cc)
target_link_libraries(test_dpc_util_subrange PRIVATE doctest::doctest dpc)
doctest_discover_tests(test_dpc_util_subrange TEST_PREFIX "util/subrange::")

# doctest_discover_tests(test_dpc_util_subrange EXTRA_ARGS -r=compact)

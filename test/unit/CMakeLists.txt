include(FetchContent)

FetchContent_Declare(doctest GIT_REPOSITORY https://github.com/doctest/doctest.git GIT_TAG v2.4.11)
FetchContent_MakeAvailable(doctest)


add_executable(test_dpc_util_bitset test_dpc_util_bitset.cc)
target_link_libraries(test_dpc_util_bitset PRIVATE doctest::doctest dpc)

add_executable(test_dpc_util_net_addr test_dpc_util_net_addr.cc)
target_link_libraries(test_dpc_util_net_addr PRIVATE doctest::doctest dpc)

add_executable(test_dpc_util_queue_mpsc test_dpc_util_queue_mpsc.cc)
target_link_libraries(test_dpc_util_queue_mpsc PRIVATE doctest::doctest dpc)

add_executable(test_dpc_util_subrange test_dpc_util_subrange.cc)
target_link_libraries(test_dpc_util_subrange PRIVATE doctest::doctest dpc)

list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
include(doctest)

doctest_discover_tests(test_dpc_util_bitset TEST_PREFIX "util/bitset::")
doctest_discover_tests(test_dpc_util_net_addr TEST_PREFIX "util/net_addr::")
doctest_discover_tests(test_dpc_util_queue_mpsc TEST_PREFIX "util/queue::")
doctest_discover_tests(test_dpc_util_subrange TEST_PREFIX "util/subrange::")

# doctest_discover_tests(test_dpc_util_subrange EXTRA_ARGS -r=compact)

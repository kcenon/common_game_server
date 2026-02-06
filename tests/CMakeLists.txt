# Test configuration
include(GoogleTest)

# Unit tests - core
add_executable(cgs_unit_tests
    unit/version_test.cpp
)
target_link_libraries(cgs_unit_tests PRIVATE
    cgs_core
    GTest::gtest_main
)
gtest_discover_tests(cgs_unit_tests)

# Unit tests - foundation common adapter
add_executable(cgs_foundation_common_tests
    unit/foundation/common_adapter_test.cpp
)
target_link_libraries(cgs_foundation_common_tests PRIVATE
    cgs::foundation_common
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_common_tests)

# Unit tests - foundation thread adapter
add_executable(cgs_foundation_thread_tests
    unit/foundation/thread_adapter_test.cpp
)
target_link_libraries(cgs_foundation_thread_tests PRIVATE
    cgs::foundation_thread
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_thread_tests)

# Unit tests - foundation logger adapter
add_executable(cgs_foundation_logger_tests
    unit/foundation/logger_adapter_test.cpp
)
target_link_libraries(cgs_foundation_logger_tests PRIVATE
    cgs::foundation_logger
    common_system
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_logger_tests)

# Unit tests - foundation network adapter
add_executable(cgs_foundation_network_tests
    unit/foundation/network_adapter_test.cpp
)
target_link_libraries(cgs_foundation_network_tests PRIVATE
    cgs::foundation_network
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_network_tests)

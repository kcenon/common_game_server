# Test configuration
include(GoogleTest)

# Unit tests
add_executable(cgs_unit_tests
    unit/version_test.cpp
)
target_link_libraries(cgs_unit_tests PRIVATE
    cgs_core
    GTest::gtest_main
)
gtest_discover_tests(cgs_unit_tests)

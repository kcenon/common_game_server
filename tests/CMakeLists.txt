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

# Unit tests - foundation database adapter
add_executable(cgs_foundation_database_tests
    unit/foundation/database_adapter_test.cpp
)
target_link_libraries(cgs_foundation_database_tests PRIVATE
    cgs::foundation_database
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_database_tests)

# Unit tests - foundation monitoring adapter
add_executable(cgs_foundation_monitoring_tests
    unit/foundation/monitoring_adapter_test.cpp
)
target_link_libraries(cgs_foundation_monitoring_tests PRIVATE
    cgs::foundation_monitoring
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_monitoring_tests)

# Unit tests - foundation container adapter
add_executable(cgs_foundation_container_tests
    unit/foundation/container_adapter_test.cpp
)
target_link_libraries(cgs_foundation_container_tests PRIVATE
    cgs::foundation_container
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_container_tests)

# Unit tests - ECS component storage
add_executable(cgs_ecs_component_storage_tests
    unit/ecs/component_storage_test.cpp
)
target_link_libraries(cgs_ecs_component_storage_tests PRIVATE
    cgs::ecs_component_storage
    GTest::gtest_main
)
gtest_discover_tests(cgs_ecs_component_storage_tests)

# Unit tests - ECS entity manager
add_executable(cgs_ecs_entity_manager_tests
    unit/ecs/entity_manager_test.cpp
)
target_link_libraries(cgs_ecs_entity_manager_tests PRIVATE
    cgs::ecs_entity_manager
    GTest::gtest_main
)
gtest_discover_tests(cgs_ecs_entity_manager_tests)

# Unit tests - ECS system scheduler
add_executable(cgs_ecs_system_scheduler_tests
    unit/ecs/system_scheduler_test.cpp
)
target_link_libraries(cgs_ecs_system_scheduler_tests PRIVATE
    cgs::ecs_system_scheduler
    GTest::gtest_main
)
gtest_discover_tests(cgs_ecs_system_scheduler_tests)

# Unit tests - ECS component query system
add_executable(cgs_ecs_query_tests
    unit/ecs/query_test.cpp
)
target_link_libraries(cgs_ecs_query_tests PRIVATE
    cgs::ecs_query
    cgs::ecs_entity_manager
    GTest::gtest_main
)
gtest_discover_tests(cgs_ecs_query_tests)

# Unit tests - ECS parallel execution
add_executable(cgs_ecs_parallel_execution_tests
    unit/ecs/parallel_execution_test.cpp
)
target_link_libraries(cgs_ecs_parallel_execution_tests PRIVATE
    cgs::ecs_system_scheduler
    GTest::gtest_main
)
gtest_discover_tests(cgs_ecs_parallel_execution_tests)

# Unit tests - Plugin system
add_executable(cgs_plugin_tests
    unit/plugin/plugin_test.cpp
)
target_link_libraries(cgs_plugin_tests PRIVATE
    cgs::plugin
    GTest::gtest_main
)
gtest_discover_tests(cgs_plugin_tests)

# Unit tests - Plugin dependency resolution
add_executable(cgs_plugin_dependency_tests
    unit/plugin/dependency_resolution_test.cpp
)
target_link_libraries(cgs_plugin_dependency_tests PRIVATE
    cgs::plugin
    GTest::gtest_main
)
gtest_discover_tests(cgs_plugin_dependency_tests)

# Unit tests - Game object system
add_executable(cgs_game_object_system_tests
    unit/game/object_system_test.cpp
)
target_link_libraries(cgs_game_object_system_tests PRIVATE
    cgs::game_object_system
    cgs::ecs_entity_manager
    GTest::gtest_main
)
gtest_discover_tests(cgs_game_object_system_tests)

# Unit tests - Game combat system
add_executable(cgs_game_combat_system_tests
    unit/game/combat_system_test.cpp
)
target_link_libraries(cgs_game_combat_system_tests PRIVATE
    cgs::game_combat_system
    cgs::ecs_entity_manager
    GTest::gtest_main
)
gtest_discover_tests(cgs_game_combat_system_tests)

# Integration tests - foundation network
add_executable(cgs_foundation_network_integration_tests
    integration/foundation/network_integration_test.cpp
)
target_link_libraries(cgs_foundation_network_integration_tests PRIVATE
    cgs::foundation_network
    NetworkSystem
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_network_integration_tests
    PROPERTIES LABELS "integration"
)

# Benchmark tests - foundation network throughput
add_executable(cgs_foundation_network_throughput_tests
    benchmark/foundation/network_throughput_test.cpp
)
target_link_libraries(cgs_foundation_network_throughput_tests PRIVATE
    cgs::foundation_network
    NetworkSystem
    GTest::gtest_main
)
gtest_discover_tests(cgs_foundation_network_throughput_tests
    PROPERTIES LABELS "benchmark"
)

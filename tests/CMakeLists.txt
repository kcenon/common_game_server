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

# Unit tests - Plugin event bus (pub/sub, priority, deferred, lifecycle events)
add_executable(cgs_plugin_event_bus_tests
    unit/plugin/event_bus_test.cpp
)
target_link_libraries(cgs_plugin_event_bus_tests PRIVATE
    cgs::plugin
    GTest::gtest_main
)
gtest_discover_tests(cgs_plugin_event_bus_tests)

# Unit tests - Plugin hot reload (file watcher, hot reload manager, state preservation)
add_executable(cgs_plugin_hot_reload_tests
    unit/plugin/hot_reload_test.cpp
)
target_link_libraries(cgs_plugin_hot_reload_tests PRIVATE
    cgs::plugin
    GTest::gtest_main
)
gtest_discover_tests(cgs_plugin_hot_reload_tests)

# Unit tests - MMORPG reference plugin (character, guild, chat, simulation)
add_executable(cgs_plugin_mmorpg_tests
    unit/plugin/mmorpg_plugin_test.cpp
)
target_link_libraries(cgs_plugin_mmorpg_tests PRIVATE
    cgs::plugin_mmorpg
    GTest::gtest_main
)
gtest_discover_tests(cgs_plugin_mmorpg_tests)

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

# Unit tests - Game world system
add_executable(cgs_game_world_system_tests
    unit/game/world_system_test.cpp
)
target_link_libraries(cgs_game_world_system_tests PRIVATE
    cgs::game_world_system
    cgs::ecs_entity_manager
    GTest::gtest_main
)
gtest_discover_tests(cgs_game_world_system_tests)

# Unit tests - Game AI system
add_executable(cgs_game_ai_system_tests
    unit/game/ai_system_test.cpp
)
target_link_libraries(cgs_game_ai_system_tests PRIVATE
    cgs::game_ai_system
    cgs::ecs_entity_manager
    GTest::gtest_main
)
gtest_discover_tests(cgs_game_ai_system_tests)

# Unit tests - Game quest system
add_executable(cgs_game_quest_system_tests
    unit/game/quest_system_test.cpp
)
target_link_libraries(cgs_game_quest_system_tests PRIVATE
    cgs::game_quest_system
    cgs::ecs_entity_manager
    GTest::gtest_main
)
gtest_discover_tests(cgs_game_quest_system_tests)

# Unit tests - Game inventory system
add_executable(cgs_game_inventory_system_tests
    unit/game/inventory_system_test.cpp
)
target_link_libraries(cgs_game_inventory_system_tests PRIVATE
    cgs::game_inventory_system
    cgs::ecs_entity_manager
    GTest::gtest_main
)
gtest_discover_tests(cgs_game_inventory_system_tests)

# Unit tests - Service auth (password hashing, token provider)
add_executable(cgs_service_auth_tests
    unit/service/auth_crypto_test.cpp
)
target_link_libraries(cgs_service_auth_tests PRIVATE
    cgs::service_auth
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_auth_tests)

# Unit tests - Service auth server (registration, login, token management)
add_executable(cgs_service_auth_server_tests
    unit/service/auth_server_test.cpp
)
target_link_libraries(cgs_service_auth_server_tests PRIVATE
    cgs::service_auth
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_auth_server_tests)

# Unit tests - Service gateway components (session manager, token bucket, route table)
add_executable(cgs_service_gateway_tests
    unit/service/gateway_components_test.cpp
)
target_link_libraries(cgs_service_gateway_tests PRIVATE
    cgs::service_gateway
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_gateway_tests)

# Unit tests - Service gateway server (connection, auth, routing, migration)
add_executable(cgs_service_gateway_server_tests
    unit/service/gateway_server_test.cpp
)
target_link_libraries(cgs_service_gateway_server_tests PRIVATE
    cgs::service_gateway
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_gateway_server_tests)

# Integration tests - Service gateway (full lifecycle, benchmark)
add_executable(cgs_service_gateway_integration_tests
    integration/service/gateway_integration_test.cpp
)
target_link_libraries(cgs_service_gateway_integration_tests PRIVATE
    cgs::service_gateway
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_gateway_integration_tests
    PROPERTIES LABELS "integration"
)

# Unit tests - Service game (game loop, map instance manager)
add_executable(cgs_service_game_tests
    unit/service/game_loop_test.cpp
)
target_link_libraries(cgs_service_game_tests PRIVATE
    cgs::service_game
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_game_tests)

# Unit tests - Service game server (world simulation, player lifecycle)
add_executable(cgs_service_game_server_tests
    unit/service/game_server_test.cpp
)
target_link_libraries(cgs_service_game_server_tests PRIVATE
    cgs::service_game
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_game_server_tests)

# Integration tests - Service game server (multi-player simulation, benchmark)
add_executable(cgs_service_game_server_integration_tests
    integration/service/game_server_integration_test.cpp
)
target_link_libraries(cgs_service_game_server_integration_tests PRIVATE
    cgs::service_game
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_game_server_integration_tests
    PROPERTIES LABELS "integration"
)

# Unit tests - Service lobby (matchmaking queue, ELO calculator)
add_executable(cgs_service_lobby_tests
    unit/service/matchmaking_test.cpp
)
target_link_libraries(cgs_service_lobby_tests PRIVATE
    cgs::service_lobby
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_lobby_tests)

# Unit tests - Service party manager (party lifecycle, member management)
add_executable(cgs_service_party_manager_tests
    unit/service/party_manager_test.cpp
)
target_link_libraries(cgs_service_party_manager_tests PRIVATE
    cgs::service_lobby
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_party_manager_tests)

# Unit tests - Service lobby server (orchestration, party queuing, ELO)
add_executable(cgs_service_lobby_server_tests
    unit/service/lobby_server_test.cpp
)
target_link_libraries(cgs_service_lobby_server_tests PRIVATE
    cgs::service_lobby
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_lobby_server_tests)

# Integration tests - Service lobby (matchmaking flow, party queuing, benchmark)
add_executable(cgs_service_lobby_integration_tests
    integration/service/lobby_integration_test.cpp
)
target_link_libraries(cgs_service_lobby_integration_tests PRIVATE
    cgs::service_lobby
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_lobby_integration_tests
    PROPERTIES LABELS "integration"
)

# Unit tests - Service dbproxy (query cache, connection pool, dbproxy server)
add_executable(cgs_service_dbproxy_tests
    unit/service/dbproxy_test.cpp
)
target_link_libraries(cgs_service_dbproxy_tests PRIVATE
    cgs::service_dbproxy
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_dbproxy_tests)

# Integration tests - service dbproxy (requires CGS_TEST_DB_CONN env var)
add_executable(cgs_service_dbproxy_integration_tests
    integration/service/dbproxy_integration_test.cpp
)
target_link_libraries(cgs_service_dbproxy_integration_tests PRIVATE
    cgs::service_dbproxy
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_dbproxy_integration_tests
    PROPERTIES LABELS "integration"
)

# Benchmark tests - service dbproxy cache latency
add_executable(cgs_service_dbproxy_benchmark_tests
    benchmark/service/dbproxy_benchmark_test.cpp
)
target_link_libraries(cgs_service_dbproxy_benchmark_tests PRIVATE
    cgs::service_dbproxy
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_dbproxy_benchmark_tests
    PROPERTIES LABELS "benchmark"
)

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

# Benchmark tests - ECS component storage throughput (SRS-NFR-003)
add_executable(cgs_ecs_component_storage_benchmark_tests
    benchmark/ecs/component_storage_benchmark_test.cpp
)
target_link_libraries(cgs_ecs_component_storage_benchmark_tests PRIVATE
    cgs::ecs_entity_manager
    cgs::game_object_system
    GTest::gtest_main
)
gtest_discover_tests(cgs_ecs_component_storage_benchmark_tests
    PROPERTIES LABELS "benchmark"
)

# Benchmark tests - ECS system scheduler and world tick (SRS-NFR-002)
add_executable(cgs_ecs_system_scheduler_benchmark_tests
    benchmark/ecs/system_scheduler_benchmark_test.cpp
)
target_link_libraries(cgs_ecs_system_scheduler_benchmark_tests PRIVATE
    cgs::plugin_mmorpg
    GTest::gtest_main
)
gtest_discover_tests(cgs_ecs_system_scheduler_benchmark_tests
    PROPERTIES LABELS "benchmark"
)

# Benchmark tests - plugin load time (SRS-NFR-006)
add_executable(cgs_plugin_load_benchmark_tests
    benchmark/plugin/plugin_load_benchmark_test.cpp
)
target_link_libraries(cgs_plugin_load_benchmark_tests PRIVATE
    cgs::plugin_mmorpg
    GTest::gtest_main
)
gtest_discover_tests(cgs_plugin_load_benchmark_tests
    PROPERTIES LABELS "benchmark"
)

# Benchmark tests - ECS memory profiling (SRS-NFR-005)
add_executable(cgs_ecs_memory_profiling_benchmark_tests
    benchmark/ecs/memory_profiling_benchmark_test.cpp
)
target_link_libraries(cgs_ecs_memory_profiling_benchmark_tests PRIVATE
    cgs::plugin_mmorpg
    GTest::gtest_main
)
gtest_discover_tests(cgs_ecs_memory_profiling_benchmark_tests
    PROPERTIES LABELS "benchmark"
)

# Benchmark tests - message serialization throughput (SRS-NFR-001)
add_executable(cgs_message_serialization_benchmark_tests
    benchmark/foundation/message_serialization_benchmark_test.cpp
)
target_link_libraries(cgs_message_serialization_benchmark_tests PRIVATE
    cgs::foundation_network
    cgs::foundation_container
    GTest::gtest_main
)
gtest_discover_tests(cgs_message_serialization_benchmark_tests
    PROPERTIES LABELS "benchmark"
)

# Benchmark tests - GameServer CCU (SRS-NFR-008, SRS-NFR-010)
add_executable(cgs_game_ccu_benchmark_tests
    benchmark/service/game_ccu_benchmark_test.cpp
)
target_link_libraries(cgs_game_ccu_benchmark_tests PRIVATE
    cgs::service_game
    GTest::gtest_main
)
gtest_discover_tests(cgs_game_ccu_benchmark_tests
    PROPERTIES LABELS "benchmark"
)

# Unit tests - Service reliability (circuit breaker, health server)
add_executable(cgs_service_reliability_tests
    unit/service/reliability_test.cpp
)
target_link_libraries(cgs_service_reliability_tests PRIVATE
    cgs::service_runner
    cgs::foundation_monitoring
    GTest::gtest_main
)
gtest_discover_tests(cgs_service_reliability_tests)

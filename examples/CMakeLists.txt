# examples/CMakeLists.txt
#
# Tutorial example programs. Each source file under this directory is a
# self-contained, runnable demonstration of one subsystem covered by a
# corresponding docs/tutorial_*.dox file.
#
# To build:
#   cmake --preset debug       (adds -DCGS_BUILD_EXAMPLES=ON automatically)
#   cmake --build build-debug
#
# Or manually:
#   cmake -B build -DCGS_BUILD_EXAMPLES=ON
#   cmake --build build
#
# Output binaries land in ${CMAKE_BINARY_DIR}/examples/cgs_example_*.

find_package(Threads REQUIRED)

# Helper: define an example executable with a common set of flags and include
# paths. Callers then add target_link_libraries() to pull in the specific
# CGS libraries each example needs.
function(cgs_add_example name source)
    add_executable(${name} ${source})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_link_libraries(${name} PRIVATE
        cgs_core
        cgs_warnings
        Threads::Threads
    )
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endfunction()

# ── Tutorial 01: Result<T> and error handling ──────────────────────────────
cgs_add_example(cgs_example_01_result_errors 01_result_errors.cpp)
target_link_libraries(cgs_example_01_result_errors PRIVATE
    cgs::foundation_common
)

# ── Tutorial 02: Foundation adapters and ServiceLocator ────────────────────
cgs_add_example(cgs_example_02_foundation_adapters 02_foundation_adapters.cpp)
target_link_libraries(cgs_example_02_foundation_adapters PRIVATE
    cgs::foundation_common
    cgs::foundation_logger
)

# ── Tutorial 03: Game objects, Transform, Identity, Stats, Movement ────────
cgs_add_example(cgs_example_03_game_objects 03_game_objects.cpp)
target_link_libraries(cgs_example_03_game_objects PRIVATE
    cgs::ecs_entity_manager
    cgs::ecs_component_storage
    cgs::ecs_query
    cgs::ecs_system_scheduler
    cgs::game_object_system
)

# ── Tutorial 04: Spatial index, zones, interest management ─────────────────
cgs_add_example(cgs_example_04_spatial_world 04_spatial_world.cpp)
target_link_libraries(cgs_example_04_spatial_world PRIVATE
    cgs::ecs_entity_manager
    cgs::ecs_component_storage
    cgs::ecs_query
    cgs::ecs_system_scheduler
    cgs::game_world_system
)

# ── Tutorial 05: Combat system, damage, auras, casting ─────────────────────
cgs_add_example(cgs_example_05_combat 05_combat.cpp)
target_link_libraries(cgs_example_05_combat PRIVATE
    cgs::ecs_entity_manager
    cgs::ecs_component_storage
    cgs::ecs_query
    cgs::ecs_system_scheduler
    cgs::game_combat_system
)

# ── Tutorial 06: AI behavior trees, patrol, targeting ──────────────────────
cgs_add_example(cgs_example_06_ai_behavior 06_ai_behavior.cpp)
target_link_libraries(cgs_example_06_ai_behavior PRIVATE
    cgs::ecs_entity_manager
    cgs::ecs_component_storage
    cgs::ecs_query
    cgs::ecs_system_scheduler
    cgs::game_ai_system
)

# ── Tutorial 07: Inventory and equipment slots ─────────────────────────────
cgs_add_example(cgs_example_07_inventory 07_inventory.cpp)
target_link_libraries(cgs_example_07_inventory PRIVATE
    cgs::ecs_entity_manager
    cgs::ecs_component_storage
    cgs::ecs_query
    cgs::ecs_system_scheduler
    cgs::game_inventory_system
)

# ── Tutorial 08: Quest system and objectives ───────────────────────────────
cgs_add_example(cgs_example_08_quest 08_quest.cpp)
target_link_libraries(cgs_example_08_quest PRIVATE
    cgs::ecs_entity_manager
    cgs::ecs_component_storage
    cgs::ecs_query
    cgs::ecs_system_scheduler
    cgs::game_quest_system
)

# ── Tutorial 09: Database adapter, prepared statements, transactions ───────
cgs_add_example(cgs_example_09_database 09_database.cpp)
target_link_libraries(cgs_example_09_database PRIVATE
    cgs::foundation_common
    cgs::foundation_database
)

# ── Tutorial 10: Network messages, sessions, signals ───────────────────────
cgs_add_example(cgs_example_10_networking 10_networking.cpp)
target_link_libraries(cgs_example_10_networking PRIVATE
    cgs::foundation_common
    cgs::foundation_network
)

# ── Tutorial 11: Client-side — server+client loopback round-trip ───────────
#
# Unlike example 10 (server-only offline demo), this example performs a
# real socket round-trip by running a GameNetworkManager server AND a
# kcenon::network::facade::tcp_facade client inside the same process.
# It links network_system directly to get access to the facade headers
# and the callback_adapter observer.
cgs_add_example(cgs_example_11_client 11_client.cpp)
target_link_libraries(cgs_example_11_client PRIVATE
    cgs::foundation_common
    cgs::foundation_network
    network_system
)

# Convenience aggregate target: `cmake --build build-debug --target cgs_examples`
# builds all example binaries in one step.
add_custom_target(cgs_examples
    DEPENDS
        cgs_example_01_result_errors
        cgs_example_02_foundation_adapters
        cgs_example_03_game_objects
        cgs_example_04_spatial_world
        cgs_example_05_combat
        cgs_example_06_ai_behavior
        cgs_example_07_inventory
        cgs_example_08_quest
        cgs_example_09_database
        cgs_example_10_networking
        cgs_example_11_client
)

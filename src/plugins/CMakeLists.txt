# Plugin System (SDS-MOD-020, SDS-MOD-021, SDS-MOD-018, SDS-MOD-023)
add_library(cgs_plugin
    plugin_manager.cpp
    version_constraint.cpp
    file_watcher.cpp
    hot_reload_manager.cpp
)
target_link_libraries(cgs_plugin
    PUBLIC cgs_core
)
# dlopen/dlclose on Unix; LoadLibrary/FreeLibrary on Windows.
if(UNIX)
    target_link_libraries(cgs_plugin PRIVATE ${CMAKE_DL_LIBS})
endif()
# Hot reload support (development only).
if(CGS_HOT_RELOAD)
    target_compile_definitions(cgs_plugin PUBLIC CGS_HOT_RELOAD)
    message(STATUS "Plugin hot reload: ENABLED (development)")
else()
    message(STATUS "Plugin hot reload: DISABLED (production)")
endif()
add_library(cgs::plugin ALIAS cgs_plugin)

# MMORPG Reference Plugin (Issue #29, SDS-MOD-023)
# Integrates all 6 game systems as a reference implementation.
add_library(cgs_plugin_mmorpg
    mmorpg_plugin.cpp
)
target_link_libraries(cgs_plugin_mmorpg
    PUBLIC cgs::plugin
           cgs::game_object_system
           cgs::game_combat_system
           cgs::game_world_system
           cgs::game_ai_system
           cgs::game_quest_system
           cgs::game_inventory_system
           cgs::ecs_entity_manager
)
add_library(cgs::plugin_mmorpg ALIAS cgs_plugin_mmorpg)

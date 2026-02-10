# Game World System (SDS-MOD-022)
add_library(cgs_game_world_system
    world_system.cpp
)
target_link_libraries(cgs_game_world_system
    PUBLIC cgs_ecs_component_storage
           cgs_ecs_system_scheduler
           cgs_game_object_system
)
add_library(cgs::game_world_system ALIAS cgs_game_world_system)

# Game Combat System (SDS-MOD-021)
add_library(cgs_game_combat_system
    combat_system.cpp
)
target_link_libraries(cgs_game_combat_system
    PUBLIC cgs_ecs_component_storage
           cgs_ecs_system_scheduler
           cgs_game_object_system
)
add_library(cgs::game_combat_system ALIAS cgs_game_combat_system)

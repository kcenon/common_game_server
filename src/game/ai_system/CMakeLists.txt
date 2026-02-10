# Game AI System (SDS-MOD-023)
add_library(cgs_game_ai_system
    ai_system.cpp
)
target_link_libraries(cgs_game_ai_system
    PUBLIC cgs_ecs_component_storage
           cgs_ecs_system_scheduler
           cgs_game_object_system
           cgs_game_combat_system
)
add_library(cgs::game_ai_system ALIAS cgs_game_ai_system)

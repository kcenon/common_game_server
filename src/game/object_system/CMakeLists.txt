# Game Object System (SDS-MOD-020)
add_library(cgs_game_object_system
    object_system.cpp
)
target_link_libraries(cgs_game_object_system
    PUBLIC cgs_ecs_component_storage
           cgs_ecs_query
           cgs_ecs_system_scheduler
)
add_library(cgs::game_object_system ALIAS cgs_game_object_system)

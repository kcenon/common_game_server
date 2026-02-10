# Game Quest System (SDS-MOD-034)
add_library(cgs_game_quest_system
    quest_system.cpp
)
target_link_libraries(cgs_game_quest_system
    PUBLIC cgs_ecs_component_storage
           cgs_ecs_system_scheduler
           cgs_game_object_system
)
add_library(cgs::game_quest_system ALIAS cgs_game_quest_system)

# Game Service - Game Loop, Map Instance Manager & Game Server (SDS-MOD-032)
add_library(cgs_service_game
    game_loop.cpp
    map_instance_manager.cpp
    game_server.cpp
)
target_link_libraries(cgs_service_game
    PUBLIC cgs_core
    PUBLIC cgs_ecs_component_storage
    PUBLIC cgs_ecs_entity_manager
    PUBLIC cgs_ecs_system_scheduler
    PUBLIC cgs_game_object_system
    PUBLIC cgs_game_world_system
    PUBLIC cgs_game_combat_system
    PUBLIC cgs_game_ai_system
    PUBLIC cgs_game_quest_system
    PUBLIC cgs_game_inventory_system
)
add_library(cgs::service_game ALIAS cgs_service_game)

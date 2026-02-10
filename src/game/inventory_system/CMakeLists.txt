# Game Inventory System (SDS-MOD-035)
add_library(cgs_game_inventory_system
    inventory_system.cpp
)
target_link_libraries(cgs_game_inventory_system
    PUBLIC cgs_ecs_component_storage
           cgs_ecs_system_scheduler
           cgs_game_object_system
)
add_library(cgs::game_inventory_system ALIAS cgs_game_inventory_system)

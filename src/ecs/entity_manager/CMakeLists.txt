# ECS Entity Manager (SDS-MOD-010)
add_library(cgs_ecs_entity_manager
    entity_manager.cpp
)
target_link_libraries(cgs_ecs_entity_manager
    PUBLIC cgs_ecs_component_storage
)
add_library(cgs::ecs_entity_manager ALIAS cgs_ecs_entity_manager)

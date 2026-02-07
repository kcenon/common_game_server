# ECS Component Storage (SDS-MOD-011)
add_library(cgs_ecs_component_storage
    component_storage.cpp
)
target_link_libraries(cgs_ecs_component_storage
    PUBLIC cgs_core
)
add_library(cgs::ecs_component_storage ALIAS cgs_ecs_component_storage)

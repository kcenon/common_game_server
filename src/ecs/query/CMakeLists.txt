# ECS Component Query System (SDS-MOD-013)
add_library(cgs_ecs_query
    query.cpp
)
target_link_libraries(cgs_ecs_query
    PUBLIC cgs_ecs_component_storage
)
add_library(cgs::ecs_query ALIAS cgs_ecs_query)

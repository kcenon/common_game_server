# ECS System Scheduler (SDS-MOD-012)
add_library(cgs_ecs_system_scheduler
    system_scheduler.cpp
)
target_link_libraries(cgs_ecs_system_scheduler
    PUBLIC cgs_ecs_component_storage
)
add_library(cgs::ecs_system_scheduler ALIAS cgs_ecs_system_scheduler)

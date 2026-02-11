# Service Runner - shared entry-point utilities (signal handling, config loading)
add_library(cgs_service_runner STATIC
    service_runner.cpp
)
target_link_libraries(cgs_service_runner
    PUBLIC cgs_core
    PUBLIC cgs_foundation_common
)
add_library(cgs::service_runner ALIAS cgs_service_runner)

# Service Runner - shared entry-point utilities (signal handling, config loading,
# health server, circuit breaker)
add_library(cgs_service_runner STATIC
    service_runner.cpp
    circuit_breaker.cpp
    health_server.cpp
)
target_link_libraries(cgs_service_runner
    PUBLIC cgs_core
    PUBLIC cgs_foundation_common
    PUBLIC cgs_foundation_monitoring
)
add_library(cgs::service_runner ALIAS cgs_service_runner)

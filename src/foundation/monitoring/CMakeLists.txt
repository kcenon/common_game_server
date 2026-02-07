# Foundation Monitoring Adapter (SDS-MOD-006)
add_library(cgs_foundation_monitoring
    game_metrics.cpp
)
target_link_libraries(cgs_foundation_monitoring
    PUBLIC cgs_core
)
add_library(cgs::foundation_monitoring ALIAS cgs_foundation_monitoring)

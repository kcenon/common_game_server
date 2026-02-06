# Foundation Logger Adapter (SDS-MOD-003)
add_library(cgs_foundation_logger
    game_logger.cpp
)
target_link_libraries(cgs_foundation_logger
    PUBLIC cgs_core
    PRIVATE common_system
)
add_library(cgs::foundation_logger ALIAS cgs_foundation_logger)

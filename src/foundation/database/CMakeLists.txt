# Foundation Database Adapter (SDS-MOD-005)
add_library(cgs_foundation_database
    game_database.cpp
)
target_link_libraries(cgs_foundation_database
    PUBLIC cgs_core
    PRIVATE database
)
add_library(cgs::foundation_database ALIAS cgs_foundation_database)

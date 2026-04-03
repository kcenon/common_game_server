# Foundation Network Adapter (SDS-MOD-004)
add_library(cgs_foundation_network
    game_network_manager.cpp
)
target_link_libraries(cgs_foundation_network
    PUBLIC cgs_core
    PRIVATE network_system
)
add_library(cgs::foundation_network ALIAS cgs_foundation_network)

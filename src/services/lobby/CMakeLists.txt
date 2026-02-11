# Lobby Service - Matchmaking, Party & Lobby Server (SDS-MOD-033)
add_library(cgs_service_lobby
    elo_calculator.cpp
    matchmaking_queue.cpp
    party_manager.cpp
    lobby_server.cpp
)
target_link_libraries(cgs_service_lobby
    PUBLIC cgs_core
)
add_library(cgs::service_lobby ALIAS cgs_service_lobby)

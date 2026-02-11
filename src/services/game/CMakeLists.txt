# Game Service - Game Loop & Map Instance Manager (SDS-MOD-032)
add_library(cgs_service_game
    game_loop.cpp
    map_instance_manager.cpp
)
target_link_libraries(cgs_service_game
    PUBLIC cgs_core
)
add_library(cgs::service_game ALIAS cgs_service_game)

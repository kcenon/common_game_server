# Foundation Container Adapter (SDS-MOD-007)
add_library(cgs_foundation_container
    game_serializer.cpp
)
target_link_libraries(cgs_foundation_container
    PUBLIC cgs_core
)
add_library(cgs::foundation_container ALIAS cgs_foundation_container)

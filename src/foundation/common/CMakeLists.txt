# Foundation Common Adapter (SDS-MOD-001)
add_library(cgs_foundation_common
    config_manager.cpp
)
target_link_libraries(cgs_foundation_common
    PUBLIC cgs_core
    PRIVATE yaml-cpp::yaml-cpp
)
add_library(cgs::foundation_common ALIAS cgs_foundation_common)

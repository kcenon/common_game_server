# Auth Service - Types, Password Hashing, Token Provider (SDS-MOD-040)
add_library(cgs_service_auth
    password_hasher.cpp
    token_provider.cpp
)
target_include_directories(cgs_service_auth
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(cgs_service_auth
    PUBLIC cgs_core
)
add_library(cgs::service_auth ALIAS cgs_service_auth)

# Auth Service - Authentication Server (SDS-MOD-040)
add_library(cgs_service_auth
    password_hasher.cpp
    token_provider.cpp
    user_repository.cpp
    token_store.cpp
    rate_limiter.cpp
    auth_server.cpp
)
target_include_directories(cgs_service_auth
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(cgs_service_auth
    PUBLIC cgs_core
)
add_library(cgs::service_auth ALIAS cgs_service_auth)

# Auth Service - Authentication Server (SDS-MOD-040)
# OpenSSL 3.x required for RS256 (RSA-SHA256) JWT signing (SRS-NFR-014).
find_package(OpenSSL 3.0.0 REQUIRED)

add_library(cgs_service_auth
    password_hasher.cpp
    token_provider.cpp
    token_blacklist.cpp
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
    PRIVATE OpenSSL::Crypto
)
add_library(cgs::service_auth ALIAS cgs_service_auth)

if(CGS_BUILD_SERVICES)
    add_executable(cgs_auth_server main.cpp)
    target_link_libraries(cgs_auth_server
        PRIVATE cgs_service_auth cgs_service_runner
    )
endif()

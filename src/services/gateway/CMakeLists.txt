# Gateway Service - Server, Session Manager, Token Bucket, Route Table (SDS-MOD-041)
add_library(cgs_service_gateway
    token_bucket.cpp
    route_table.cpp
    gateway_session_manager.cpp
    gateway_server.cpp
)
target_link_libraries(cgs_service_gateway
    PUBLIC cgs_core
    PUBLIC cgs_service_auth
)
add_library(cgs::service_gateway ALIAS cgs_service_gateway)

if(CGS_BUILD_SERVICES)
    add_executable(cgs_gateway_server main.cpp)
    target_link_libraries(cgs_gateway_server
        PRIVATE cgs_service_gateway cgs_service_runner
    )
endif()

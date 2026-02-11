# DBProxy Service - Connection Pooling, Query Caching, Replica Routing (SDS-MOD-034)
add_library(cgs_service_dbproxy
    query_cache.cpp
    connection_pool_manager.cpp
    dbproxy_server.cpp
)
target_link_libraries(cgs_service_dbproxy
    PUBLIC cgs_core
    PRIVATE cgs::foundation_database
)
add_library(cgs::service_dbproxy ALIAS cgs_service_dbproxy)

if(CGS_BUILD_SERVICES)
    add_executable(cgs_dbproxy_server main.cpp)
    target_link_libraries(cgs_dbproxy_server
        PRIVATE cgs_service_dbproxy cgs_service_runner
    )
endif()

# DBProxy Service - Connection Pooling, Query Caching, Replica Routing (SDS-MOD-034)
add_library(cgs_service_dbproxy
    query_cache.cpp
    connection_pool_manager.cpp
    dbproxy_server.cpp
)
target_link_libraries(cgs_service_dbproxy
    PUBLIC cgs_core
)
add_library(cgs::service_dbproxy ALIAS cgs_service_dbproxy)

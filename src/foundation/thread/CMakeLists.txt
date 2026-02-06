# Foundation Thread Adapter (SDS-MOD-002)
add_library(cgs_foundation_thread
    job_scheduler.cpp
)
target_link_libraries(cgs_foundation_thread
    PUBLIC cgs_core
    PRIVATE thread_system
)
add_library(cgs::foundation_thread ALIAS cgs_foundation_thread)

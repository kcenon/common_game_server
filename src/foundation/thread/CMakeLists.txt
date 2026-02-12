# Foundation Thread Adapter (SDS-MOD-002)
add_library(cgs_foundation_thread
    job_scheduler.cpp
)
target_link_libraries(cgs_foundation_thread
    PUBLIC cgs_core
    PRIVATE thread_system
)
# Apply thread_system's compile definitions to this target so that header-defined
# class layouts (e.g. lifecycle_controller) match the library's own TUs.
# See cmake/kcenon_deps.cmake for why this is needed (ODR violation fix, #80).
foreach(_def ${CGS_THREAD_SYSTEM_DEFS})
    target_compile_definitions(cgs_foundation_thread PRIVATE ${_def})
endforeach()
add_library(cgs::foundation_thread ALIAS cgs_foundation_thread)

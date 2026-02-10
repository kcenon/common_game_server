# Plugin System (SDS-MOD-020, SDS-MOD-021)
add_library(cgs_plugin
    plugin_manager.cpp
)
target_link_libraries(cgs_plugin
    PUBLIC cgs_core
)
# dlopen/dlclose on Unix; LoadLibrary/FreeLibrary on Windows.
if(UNIX)
    target_link_libraries(cgs_plugin PRIVATE ${CMAKE_DL_LIBS})
endif()
add_library(cgs::plugin ALIAS cgs_plugin)

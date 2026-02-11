# Layer 3: Services
add_subdirectory(auth)
add_subdirectory(gateway)
add_subdirectory(game)
add_subdirectory(lobby)
add_subdirectory(dbproxy)

# Shared entry-point utilities (always built when services are requested)
if(CGS_BUILD_SERVICES)
    add_subdirectory(runner)
endif()

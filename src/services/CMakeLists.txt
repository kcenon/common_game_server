# Layer 3: Services
add_subdirectory(auth)
add_subdirectory(gateway)
add_subdirectory(game)
add_subdirectory(lobby)
add_subdirectory(dbproxy)

# Shared entry-point utilities (always built — tests depend on this library)
add_subdirectory(runner)

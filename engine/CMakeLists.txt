# engine/CMakeLists.txt
# Engine umbrella - adds all modules

add_subdirectory(core)
add_subdirectory(reflect)
add_subdirectory(scene)
add_subdirectory(physics)
add_subdirectory(audio)
add_subdirectory(plugin)

# Umbrella target for convenience
add_library(engine INTERFACE)
add_library(engine::engine ALIAS engine)

# On GCC/Clang with static libraries, circular dependencies between engine
# modules require a linker group so the linker resolves all symbols.
if(UNIX)
    target_link_libraries(engine INTERFACE
        -Wl,--start-group
        engine::core
        engine::reflect
        engine::scene


        engine::physics
        engine::audio
        engine::plugin
        -Wl,--end-group
    )
else()
    target_link_libraries(engine INTERFACE
        engine::core
        engine::reflect
        engine::scene


        engine::physics
        engine::audio
        engine::plugin
    )
endif()

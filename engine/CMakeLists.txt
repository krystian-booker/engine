# engine/CMakeLists.txt
# Engine umbrella - adds all modules

add_subdirectory(core)
add_subdirectory(reflect)
add_subdirectory(scene)
add_subdirectory(physics)
add_subdirectory(audio)
add_subdirectory(plugin)
add_subdirectory(app)

# Umbrella target for convenience
add_library(engine INTERFACE)
add_library(engine::engine ALIAS engine)

target_link_libraries(engine INTERFACE
    engine::core
    engine::reflect
    engine::scene
    engine::physics
    engine::audio
    engine::plugin
    engine::app
)

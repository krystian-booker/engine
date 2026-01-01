# engine/CMakeLists.txt
# Engine umbrella - adds all modules

add_subdirectory(core)
add_subdirectory(scene)
add_subdirectory(render)
add_subdirectory(asset)
add_subdirectory(physics)
add_subdirectory(audio)
add_subdirectory(navigation)
add_subdirectory(ui)
add_subdirectory(debug)

# Umbrella target for convenience
add_library(engine INTERFACE)
add_library(engine::engine ALIAS engine)

target_link_libraries(engine INTERFACE
    engine::core
    engine::scene
    engine::render
    engine::asset
    engine::physics
    engine::audio
    engine::navigation
    engine::ui
    engine::debug
)

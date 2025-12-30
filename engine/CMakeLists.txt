# engine/CMakeLists.txt
# Engine umbrella - adds all modules

add_subdirectory(core)
add_subdirectory(render)
add_subdirectory(asset)

# Umbrella target for convenience
add_library(engine INTERFACE)
add_library(engine::engine ALIAS engine)

target_link_libraries(engine INTERFACE
    engine::core
    engine::render
    engine::asset
)

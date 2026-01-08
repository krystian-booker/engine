# engine/CMakeLists.txt
# Engine umbrella - adds all modules

add_subdirectory(core)
add_subdirectory(reflect)
add_subdirectory(scene)
add_subdirectory(stats)
add_subdirectory(plugin)
add_subdirectory(effects)
add_subdirectory(inventory)
add_subdirectory(combat)
add_subdirectory(script)
add_subdirectory(render)
add_subdirectory(asset)
add_subdirectory(physics)
add_subdirectory(audio)
add_subdirectory(navigation)
add_subdirectory(ai)
add_subdirectory(quest)
add_subdirectory(dialogue)
add_subdirectory(ui)
add_subdirectory(debug-gui)
add_subdirectory(save)
add_subdirectory(localization)
add_subdirectory(streaming)
add_subdirectory(terrain)
add_subdirectory(vegetation)
add_subdirectory(cinematic)
add_subdirectory(spline)
# add_subdirectory(plugin) moved to line 7
add_subdirectory(data)
add_subdirectory(settings)
add_subdirectory(achievements)
add_subdirectory(environment)

# Umbrella target for convenience
add_library(engine INTERFACE)
add_library(engine::engine ALIAS engine)

target_link_libraries(engine INTERFACE
    engine::core
    engine::reflect
    engine::scene
    engine::stats
    engine::effects
    engine::inventory
    engine::combat
    engine::script
    engine::render
    engine::asset
    engine::physics
    engine::audio
    engine::navigation
    engine::ai
    engine::quest
    engine::dialogue
    engine::ui
    engine::debug-gui
    engine::save
    engine::localization
    engine::streaming
    engine::terrain
    engine::vegetation
    engine::cinematic
    engine::spline
    engine::plugin
    engine::data
    engine::settings
    engine::achievements
    engine::environment
)

# engine/script/CMakeLists.txt
# engine::script - Lua scripting integration

add_library(engine_script STATIC)
add_library(engine::script ALIAS engine_script)

target_sources(engine_script
    PRIVATE
        src/lua_state.cpp
        src/script_component.cpp
        src/script_context.cpp
        src/bindings.cpp
        src/math_bindings.cpp
        src/entity_bindings.cpp
        src/input_bindings.cpp
        src/localization_bindings.cpp
        src/physics_bindings.cpp
        src/audio_bindings.cpp
        src/navigation_bindings.cpp
        src/debug_bindings.cpp
        src/camera_bindings.cpp
        src/animation_bindings.cpp
        src/save_bindings.cpp
        src/scene_bindings.cpp
        src/ui_bindings.cpp
        src/render_bindings.cpp
        src/cinematic_bindings.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/script/script.hpp
            include/engine/script/lua_state.hpp
            include/engine/script/script_component.hpp
            include/engine/script/script_context.hpp
            include/engine/script/bindings.hpp
)

target_include_directories(engine_script
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_script
    PUBLIC
        engine::core
        engine::reflect
        engine::scene
        sol2::sol2
    PRIVATE
        engine_compiler_flags
        engine::localization
        engine::physics
        engine::audio
        engine::navigation
        engine::render
        engine::save
        engine::ui
        engine::cinematic
)

set_target_properties(engine_script PROPERTIES
    FOLDER "engine"
)

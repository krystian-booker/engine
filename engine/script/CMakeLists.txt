# engine/script/CMakeLists.txt
# engine::script - Lua scripting integration

add_library(engine_script STATIC)
add_library(engine::script ALIAS engine_script)

target_sources(engine_script
    PRIVATE
        src/lua_state.cpp
        src/script_component.cpp
        src/bindings.cpp
        src/math_bindings.cpp
        src/entity_bindings.cpp
        src/input_bindings.cpp
        src/localization_bindings.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/script/script.hpp
            include/engine/script/lua_state.hpp
            include/engine/script/script_component.hpp
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
)

set_target_properties(engine_script PROPERTIES
    FOLDER "engine"
)

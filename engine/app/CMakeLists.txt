# engine/app/CMakeLists.txt
# engine::app - application framework (game loop, plugin orchestration)

add_library(engine_app STATIC)
add_library(engine::app ALIAS engine_app)

target_sources(engine_app
    PRIVATE
        src/application.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/app/application.hpp
)

target_include_directories(engine_app
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_app
    PUBLIC
        engine::core
        engine::scene
        engine::physics
        engine::audio
        engine::plugin
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_app PROPERTIES
    FOLDER "engine"
)

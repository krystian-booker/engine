# engine/app/CMakeLists.txt
# engine::app - application framework (game loop, plugin orchestration)

engine_add_module(
    NAME app
    SOURCES
        src/application.cpp
    HEADERS
        include/engine/app/application.hpp
    PUBLIC_DEPS
        engine::core
        engine::scene
        engine::physics
        engine::audio
        engine::plugin
)

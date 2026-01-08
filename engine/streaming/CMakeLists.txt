# engine/streaming/CMakeLists.txt
# engine::streaming - Scene streaming and level loading

add_library(engine_streaming STATIC)
add_library(engine::streaming ALIAS engine_streaming)

target_sources(engine_streaming
    PRIVATE
        src/scene_streaming.cpp
        src/streaming_systems.cpp
        src/streaming_component_registration.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/streaming/streaming.hpp
            include/engine/streaming/scene_streaming.hpp
            include/engine/streaming/streaming_volume.hpp
)

target_include_directories(engine_streaming
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_streaming
    PUBLIC
        engine::core
        engine::scene
        engine::asset
    PRIVATE
        engine::reflect
        engine_compiler_flags
)

set_target_properties(engine_streaming PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

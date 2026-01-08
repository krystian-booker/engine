# engine/dialogue/CMakeLists.txt

add_library(engine_dialogue STATIC
    src/dialogue_graph.cpp
    src/dialogue_player.cpp
    src/dialogue_systems.cpp
)
add_library(engine::dialogue ALIAS engine_dialogue)

target_include_directories(engine_dialogue
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_dialogue
    PUBLIC
        engine::core
        engine::scene
        engine::localization
        engine::audio
        engine::reflect
)

target_compile_features(engine_dialogue PUBLIC cxx_std_20)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

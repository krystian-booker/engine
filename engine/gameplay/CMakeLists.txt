# engine/gameplay/CMakeLists.txt
# Character movement states, traversal mechanics

add_library(engine_gameplay STATIC
    src/character_movement.cpp
    src/water_volume.cpp
)
add_library(engine::gameplay ALIAS engine_gameplay)

target_include_directories(engine_gameplay
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_gameplay
    PUBLIC
        engine::core
        engine::scene
        engine::physics
        engine::render
        engine::stats
    PRIVATE
        engine_compiler_flags
)

target_compile_features(engine_gameplay PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(engine_gameplay PROPERTIES FOLDER "Engine")

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

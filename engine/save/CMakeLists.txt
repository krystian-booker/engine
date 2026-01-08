# engine/save/CMakeLists.txt
# engine::save - Save/Load system for game state persistence

add_library(engine_save STATIC)
add_library(engine::save ALIAS engine_save)

target_sources(engine_save
    PRIVATE
        src/save_system.cpp
        src/save_game.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/save/save.hpp
            include/engine/save/save_system.hpp
            include/engine/save/save_game.hpp
)

target_include_directories(engine_save
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_save
    PUBLIC
        engine::core
        engine::scene
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_save PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

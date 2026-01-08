# engine/combat/CMakeLists.txt
# engine::combat - Hitbox/hurtbox detection, damage system, i-frames, attack phases

add_library(engine_combat STATIC)
add_library(engine::combat ALIAS engine_combat)

target_sources(engine_combat
    PRIVATE
        src/hitbox.cpp
        src/damage.cpp
        src/combat_systems.cpp
        src/hit_reaction.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/combat/combat.hpp
            include/engine/combat/hitbox.hpp
            include/engine/combat/hurtbox.hpp
            include/engine/combat/damage.hpp
            include/engine/combat/iframe.hpp
            include/engine/combat/attack_phases.hpp
            include/engine/combat/hit_reaction.hpp
)

target_include_directories(engine_combat
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_combat
    PUBLIC
        engine::core
        engine::scene
        engine::physics
        engine::render
        engine::stats
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_combat PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

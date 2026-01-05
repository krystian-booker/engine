# engine/core/CMakeLists.txt
# engine::core - logging, job system, time, filesystem, math, settings

add_library(engine_core STATIC)
add_library(engine::core ALIAS engine_core)

target_sources(engine_core
    PRIVATE
        src/log.cpp
        src/job_system.cpp
        src/time.cpp
        src/filesystem.cpp
        src/project_settings.cpp
        src/application.cpp
        src/serialize.cpp
        src/input.cpp
        src/profiler.cpp
        src/event_dispatcher.cpp
        src/allocators.cpp
        src/uuid.cpp
        src/math.cpp
        src/scene_transition.cpp
        src/timer.cpp
        src/game_events.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/core/log.hpp
            include/engine/core/job_system.hpp
            include/engine/core/time.hpp
            include/engine/core/filesystem.hpp
            include/engine/core/math.hpp
            include/engine/core/project_settings.hpp
            include/engine/core/game_clock.hpp
            include/engine/core/application.hpp
            include/engine/core/input.hpp
            include/engine/core/serialize.hpp
            include/engine/core/profiler.hpp
            include/engine/core/core.hpp
            include/engine/core/events.hpp
            include/engine/core/event_dispatcher.hpp
            include/engine/core/allocators.hpp
            include/engine/core/string_hash.hpp
            include/engine/core/uuid.hpp
            include/engine/core/asset_handle.hpp
            include/engine/core/scene_transition.hpp
            include/engine/core/timer.hpp
            include/engine/core/game_events.hpp
            include/engine/core/state_machine.hpp
)

target_include_directories(engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_core
    PUBLIC
        glm::glm
        nlohmann_json::nlohmann_json
        bgfx::bx
        bgfx::bgfx
    PRIVATE
        engine_compiler_flags
)

# Application class requires additional engine modules
# These are linked via object library to avoid circular dependencies
# The umbrella engine::engine target will provide proper linkage
target_include_directories(engine_core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../scene/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../render/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../plugin/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../streaming/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../cinematic/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../navigation/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../ui/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../terrain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../vegetation/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../physics/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../script/include
)

# Scene headers require EnTT
target_link_libraries(engine_core
    PRIVATE
        entt::entt
)

set_target_properties(engine_core PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

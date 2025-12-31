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
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_core PROPERTIES
    FOLDER "engine"
)

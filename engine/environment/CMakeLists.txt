# engine/environment/CMakeLists.txt
# engine::environment - Weather, time of day, and environmental effects

add_library(engine_environment STATIC)
add_library(engine::environment ALIAS engine_environment)

target_sources(engine_environment
    PRIVATE
        src/time_of_day.cpp
        src/sky_controller.cpp
        src/environment_lighting.cpp
        src/weather.cpp
        src/weather_effects.cpp
        src/weather_audio.cpp
        src/environment_systems.cpp
        src/environment_registration.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/environment/environment.hpp
            include/engine/environment/time_of_day.hpp
            include/engine/environment/sky_controller.hpp
            include/engine/environment/environment_lighting.hpp
            include/engine/environment/weather.hpp
            include/engine/environment/weather_effects.hpp
            include/engine/environment/weather_audio.hpp
            include/engine/environment/environment_components.hpp
)

target_include_directories(engine_environment
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_environment
    PUBLIC
        engine::core
        engine::scene
        engine::audio
    PRIVATE
        engine::reflect
        engine::render
        engine_compiler_flags
)

set_target_properties(engine_environment PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

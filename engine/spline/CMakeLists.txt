# engine/spline/CMakeLists.txt
# engine::spline - Spline curves for paths, rails, and procedural geometry

add_library(engine_spline STATIC)
add_library(engine::spline ALIAS engine_spline)

target_sources(engine_spline
    PRIVATE
        src/spline.cpp
        src/bezier_spline.cpp
        src/catmull_rom.cpp
        src/spline_component.cpp
        src/spline_follower.cpp
        src/spline_registration.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/spline/spline.hpp
            include/engine/spline/bezier_spline.hpp
            include/engine/spline/catmull_rom.hpp
            include/engine/spline/spline_component.hpp
            include/engine/spline/spline_follower.hpp
)

target_include_directories(engine_spline
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_spline
    PUBLIC
        engine::core
        engine::scene
    PRIVATE
        engine::reflect
        engine_compiler_flags
)

set_target_properties(engine_spline PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

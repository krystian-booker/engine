# engine/stats/CMakeLists.txt

add_library(engine_stats STATIC
    src/stat_definition.cpp
    src/stat_modifier.cpp
    src/stat_component.cpp
    src/stat_calculator.cpp
    src/stat_registration.cpp
)
add_library(engine::stats ALIAS engine_stats)

target_include_directories(engine_stats
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_stats
    PUBLIC
        engine::core
        engine::scene
        engine::reflect
)

target_compile_features(engine_stats PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(engine_stats PROPERTIES FOLDER "Engine")

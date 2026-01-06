# engine/effects/CMakeLists.txt

add_library(engine_effects STATIC
    src/effect_definition.cpp
    src/effect_instance.cpp
    src/effect_component.cpp
    src/effect_manager.cpp
    src/effect_registration.cpp
)
add_library(engine::effects ALIAS engine_effects)

target_include_directories(engine_effects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_effects
    PUBLIC
        engine::core
        engine::scene
        engine::stats
        engine::reflect
)

target_compile_features(engine_effects PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(engine_effects PROPERTIES FOLDER "Engine")

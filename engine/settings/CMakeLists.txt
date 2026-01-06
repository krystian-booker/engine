# engine/settings/CMakeLists.txt

add_library(engine_settings STATIC
    src/graphics_settings.cpp
    src/audio_settings.cpp
    src/input_settings.cpp
    src/gameplay_settings.cpp
    src/settings_manager.cpp
)
add_library(engine::settings ALIAS engine_settings)

target_include_directories(engine_settings
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_settings
    PUBLIC
        engine::core
    PRIVATE
        nlohmann_json::nlohmann_json
)

target_compile_features(engine_settings PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(engine_settings PROPERTIES FOLDER "Engine")

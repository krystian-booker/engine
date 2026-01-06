# engine/achievements/CMakeLists.txt

add_library(engine_achievements STATIC
    src/achievement_definition.cpp
    src/achievement_manager.cpp
)
add_library(engine::achievements ALIAS engine_achievements)

target_include_directories(engine_achievements
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_achievements
    PUBLIC
        engine::core
    PRIVATE
        nlohmann_json::nlohmann_json
)

target_compile_features(engine_achievements PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(engine_achievements PROPERTIES FOLDER "Engine")

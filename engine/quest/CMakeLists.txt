# engine/quest/CMakeLists.txt

add_library(engine_quest STATIC
    src/quest.cpp
    src/quest_manager.cpp
    src/waypoint.cpp
    src/quest_systems.cpp
)
add_library(engine::quest ALIAS engine_quest)

target_include_directories(engine_quest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_quest
    PUBLIC
        engine::core
        engine::scene
        engine::save
        engine::reflect
)

target_compile_features(engine_quest PUBLIC cxx_std_20)

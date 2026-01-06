# engine/ai/CMakeLists.txt
# engine::ai - Behavior trees, AI perception, and blackboard system

add_library(engine_ai STATIC)
add_library(engine::ai ALIAS engine_ai)

target_sources(engine_ai
    PRIVATE
        src/blackboard.cpp
        src/behavior_tree.cpp
        src/bt_nodes.cpp
        src/perception.cpp
        src/ai_systems.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/ai/ai.hpp
            include/engine/ai/blackboard.hpp
            include/engine/ai/behavior_tree.hpp
            include/engine/ai/bt_composites.hpp
            include/engine/ai/bt_decorators.hpp
            include/engine/ai/bt_nodes.hpp
            include/engine/ai/perception.hpp
            include/engine/ai/ai_components.hpp
)

target_include_directories(engine_ai
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_ai
    PUBLIC
        engine::core
        engine::scene
        engine::navigation
        engine::physics
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_ai PROPERTIES
    FOLDER "engine"
)

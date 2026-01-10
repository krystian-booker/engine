# engine/companion/CMakeLists.txt
# AI companions, party management, and formations

add_library(engine_companion STATIC
    src/companion.cpp
    src/formation.cpp
    src/party_manager.cpp
    src/companion_ai.cpp
)
add_library(engine::companion ALIAS engine_companion)

target_include_directories(engine_companion
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_companion
    PUBLIC
        engine::core
        engine::scene
        engine::ai
        engine::navigation
        engine::physics
        engine::combat
    PRIVATE
        engine_compiler_flags
        nlohmann_json::nlohmann_json
)

target_compile_features(engine_companion PUBLIC cxx_std_20)

# Set IDE folder
set_target_properties(engine_companion PROPERTIES FOLDER "Engine")

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

# engine/audio/CMakeLists.txt
# engine::audio - miniaudio wrapper

add_library(engine_audio STATIC)
add_library(engine::audio ALIAS engine_audio)

target_sources(engine_audio
    PRIVATE
        src/audio_engine.cpp
        src/miniaudio_impl.cpp
        src/audio_components.cpp
        src/audio_system.cpp
        src/audio_component_registration.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/audio/audio.hpp
            include/engine/audio/audio_engine.hpp
            include/engine/audio/sound.hpp
            include/engine/audio/audio_components.hpp
            include/engine/audio/audio_system.hpp
)

target_include_directories(engine_audio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ma_reverb_node
)

target_link_libraries(engine_audio
    PUBLIC
        engine::core
        engine::scene
        engine::reflect
        miniaudio::miniaudio
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_audio PROPERTIES
    FOLDER "engine"
)

# Test executable
add_executable(audio_test tests/manual/audio_manual_test.cpp)
target_link_libraries(audio_test PRIVATE engine_audio)
set_target_properties(audio_test PROPERTIES FOLDER "tests")

# 3D Audio Test
add_executable(audio_3d_test tests/manual/audio_3d_test.cpp)
target_link_libraries(audio_3d_test PRIVATE engine_audio)
set_target_properties(audio_3d_test PROPERTIES FOLDER "tests")

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

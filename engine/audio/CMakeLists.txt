# engine/audio/CMakeLists.txt
# engine::audio - miniaudio wrapper

engine_add_module(
    NAME audio
    SOURCES
        src/audio_engine.cpp
        src/miniaudio_impl.cpp
        src/audio_components.cpp
        src/audio_system.cpp
        src/audio_component_registration.cpp
    HEADERS
        include/engine/audio/audio.hpp
        include/engine/audio/audio_engine.hpp
        include/engine/audio/sound.hpp
        include/engine/audio/audio_components.hpp
        include/engine/audio/audio_system.hpp
    PUBLIC_DEPS
        engine::core
        engine::scene
        engine::reflect
        miniaudio::miniaudio
)

# Private include for reverb node
target_include_directories(engine_audio
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ma_reverb_node
)

# Manual test executables
if(ENGINE_BUILD_TESTS)
    add_executable(audio_test tests/manual/audio_manual_test.cpp)
    target_link_libraries(audio_test PRIVATE engine_audio engine_compiler_flags)
    set_target_properties(audio_test PROPERTIES FOLDER "tests")

    add_executable(audio_3d_test tests/manual/audio_3d_test.cpp)
    target_link_libraries(audio_3d_test PRIVATE engine_audio engine_compiler_flags)
    set_target_properties(audio_3d_test PROPERTIES FOLDER "tests")
endif()

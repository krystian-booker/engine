# engine/audio/CMakeLists.txt
# engine::audio - miniaudio wrapper

add_library(engine_audio STATIC)
add_library(engine::audio ALIAS engine_audio)

target_sources(engine_audio
    PRIVATE
        src/audio_engine.cpp
        src/miniaudio_impl.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/audio/audio.hpp
            include/engine/audio/audio_engine.hpp
            include/engine/audio/sound.hpp
)

target_include_directories(engine_audio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_audio
    PUBLIC
        engine::core
        miniaudio::miniaudio
    PRIVATE
        engine_compiler_flags
)

set_target_properties(engine_audio PROPERTIES
    FOLDER "engine"
)

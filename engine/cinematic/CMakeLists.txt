# engine/cinematic/CMakeLists.txt
# engine::cinematic - Cinematic sequencer system

add_library(engine_cinematic STATIC)
add_library(engine::cinematic ALIAS engine_cinematic)

target_sources(engine_cinematic
    PRIVATE
        src/track.cpp
        src/camera_track.cpp
        src/animation_track.cpp
        src/audio_track.cpp
        src/event_track.cpp
        src/light_track.cpp
        src/postprocess_track.cpp
        src/sequence.cpp
        src/player.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/cinematic/cinematic.hpp
            include/engine/cinematic/track.hpp
            include/engine/cinematic/camera_track.hpp
            include/engine/cinematic/animation_track.hpp
            include/engine/cinematic/audio_track.hpp
            include/engine/cinematic/event_track.hpp
            include/engine/cinematic/light_track.hpp
            include/engine/cinematic/postprocess_track.hpp
            include/engine/cinematic/sequence.hpp
            include/engine/cinematic/player.hpp
)

target_include_directories(engine_cinematic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_cinematic
    PUBLIC
        engine::core
        engine::scene
        engine::audio
        engine::render
    PRIVATE
        engine_compiler_flags
        nlohmann_json::nlohmann_json
)

set_target_properties(engine_cinematic PROPERTIES
    FOLDER "engine"
)

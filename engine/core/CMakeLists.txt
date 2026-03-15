# engine/core/CMakeLists.txt
# engine::core - logging, job system, time, filesystem, math, settings

add_library(engine_core STATIC)
add_library(engine::core ALIAS engine_core)

target_sources(engine_core
    PRIVATE
        src/log.cpp
        src/job_system.cpp
        src/time.cpp
        src/filesystem.cpp
        src/file_watcher.cpp
        src/project_settings.cpp
        src/application.cpp
        src/platform_window.cpp
        src/x11_window.cpp
        src/win32_window.cpp
        src/wayland_window.cpp
        src/serialize.cpp
        src/input.cpp
        src/input_buffer.cpp
        src/profiler.cpp
        src/event_dispatcher.cpp
        src/allocators.cpp
        src/uuid.cpp
        src/math.cpp
        src/scene_transition.cpp
        src/timer.cpp
        src/game_events.cpp
        src/time_manager.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/core/log.hpp
            include/engine/core/job_system.hpp
            include/engine/core/time.hpp
            include/engine/core/filesystem.hpp
            include/engine/core/file_watcher.hpp
            include/engine/core/math.hpp
            include/engine/core/project_settings.hpp
            include/engine/core/game_clock.hpp
            include/engine/core/application.hpp
            include/engine/core/platform_window.hpp
            include/engine/core/input.hpp
            include/engine/core/input_buffer.hpp
            include/engine/core/serialize.hpp
            include/engine/core/profiler.hpp
            include/engine/core/core.hpp
            include/engine/core/events.hpp
            include/engine/core/event_dispatcher.hpp
            include/engine/core/allocators.hpp
            include/engine/core/string_hash.hpp
            include/engine/core/uuid.hpp
            include/engine/core/asset_handle.hpp
            include/engine/core/scene_transition.hpp
            include/engine/core/timer.hpp
            include/engine/core/game_events.hpp
            include/engine/core/state_machine.hpp
            include/engine/core/time_manager.hpp
            include/engine/core/object_pool.hpp
)

target_include_directories(engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_core
    PUBLIC
        glm::glm
        nlohmann_json::nlohmann_json
        bgfx::bx
        bgfx::bgfx
    PRIVATE
        engine_compiler_flags
)

# Application class requires additional engine modules
# These are linked via object library to avoid circular dependencies
# The umbrella engine::engine target will provide proper linkage
target_include_directories(engine_core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../scene/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../render/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../plugin/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../streaming/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../cinematic/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../navigation/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../ui/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../terrain/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../vegetation/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../physics/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../script/include
)

# Scene headers require EnTT
target_link_libraries(engine_core
    PRIVATE
        entt::entt
)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(engine_core PRIVATE X11::X11)
    target_link_libraries(engine_core PRIVATE ${CMAKE_DL_LIBS})

    # Wayland (optional — enables Wayland-first at runtime)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND_CLIENT wayland-client)
        pkg_check_modules(XKB xkbcommon)
        pkg_check_modules(WAYLAND_CURSOR wayland-cursor)
    endif()

    if(WAYLAND_CLIENT_FOUND AND XKB_FOUND)
        # Find wayland-scanner and protocol XML sources
        find_program(WAYLAND_SCANNER wayland-scanner)
        if(PkgConfig_FOUND)
            execute_process(
                COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=pkgdatadir wayland-protocols
                OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        endif()

        if(WAYLAND_SCANNER AND WAYLAND_PROTOCOLS_DIR)
            set(WL_PROTO_OUT ${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols)
            file(MAKE_DIRECTORY ${WL_PROTO_OUT})

            # xdg-shell protocol
            set(XDG_SHELL_XML ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)
            add_custom_command(
                OUTPUT ${WL_PROTO_OUT}/xdg-shell-client-protocol.h
                       ${WL_PROTO_OUT}/xdg-shell-protocol.c
                COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_XML}
                        ${WL_PROTO_OUT}/xdg-shell-client-protocol.h
                COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_XML}
                        ${WL_PROTO_OUT}/xdg-shell-protocol.c
                DEPENDS ${XDG_SHELL_XML}
                COMMENT "Generating xdg-shell Wayland protocol"
            )

            # xdg-decoration protocol
            set(XDG_DECO_XML ${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml)
            add_custom_command(
                OUTPUT ${WL_PROTO_OUT}/xdg-decoration-unstable-v1-client-protocol.h
                       ${WL_PROTO_OUT}/xdg-decoration-unstable-v1-protocol.c
                COMMAND ${WAYLAND_SCANNER} client-header ${XDG_DECO_XML}
                        ${WL_PROTO_OUT}/xdg-decoration-unstable-v1-client-protocol.h
                COMMAND ${WAYLAND_SCANNER} private-code ${XDG_DECO_XML}
                        ${WL_PROTO_OUT}/xdg-decoration-unstable-v1-protocol.c
                DEPENDS ${XDG_DECO_XML}
                COMMENT "Generating xdg-decoration Wayland protocol"
            )

            target_sources(engine_core PRIVATE
                ${WL_PROTO_OUT}/xdg-shell-protocol.c
                ${WL_PROTO_OUT}/xdg-decoration-unstable-v1-protocol.c
            )

            target_include_directories(engine_core PRIVATE ${WL_PROTO_OUT})
            target_compile_definitions(engine_core PRIVATE ENGINE_HAS_WAYLAND=1)
            target_link_libraries(engine_core PRIVATE
                ${WAYLAND_CLIENT_LIBRARIES}
                ${XKB_LIBRARIES}
            )
            if(WAYLAND_CURSOR_FOUND)
                target_link_libraries(engine_core PRIVATE ${WAYLAND_CURSOR_LIBRARIES})
            endif()
            target_include_directories(engine_core PRIVATE
                ${WAYLAND_CLIENT_INCLUDE_DIRS}
                ${XKB_INCLUDE_DIRS}
            )

            message(STATUS "Wayland support enabled (runtime Wayland-first, X11 fallback)")
        else()
            message(STATUS "Wayland support disabled (wayland-scanner or wayland-protocols not found)")
        endif()
    else()
        message(STATUS "Wayland support disabled (wayland-client or xkbcommon not found)")
    endif()
endif()

set_target_properties(engine_core PROPERTIES
    FOLDER "engine"
)

# Unit tests
if(ENGINE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

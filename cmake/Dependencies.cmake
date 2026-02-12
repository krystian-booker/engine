# Dependencies.cmake
# External dependencies: vcpkg packages via find_package() + bgfx/ufbx via FetchContent

include(FetchContent)

# ============================================================================
# vcpkg packages (installed automatically via manifest mode)
# ============================================================================

# GLM (math library)
find_package(glm CONFIG REQUIRED)
# vcpkg's glm target is glm::glm - matches our usage
# Add experimental extensions define
if(TARGET glm::glm)
    target_compile_definitions(glm::glm INTERFACE GLM_ENABLE_EXPERIMENTAL)
endif()

# nlohmann/json (serialization)
find_package(nlohmann_json CONFIG REQUIRED)
# Target: nlohmann_json::nlohmann_json - matches our usage

# stb (image loading - header only)
find_package(Stb REQUIRED)
# vcpkg provides Stb_INCLUDE_DIR, not a target - create one
if(NOT TARGET stb::stb)
    add_library(stb INTERFACE)
    add_library(stb::stb ALIAS stb)
    target_include_directories(stb INTERFACE ${Stb_INCLUDE_DIR})
endif()

# cgltf (glTF loading - header only)
find_path(CGLTF_INCLUDE_DIR NAMES cgltf.h)
if(NOT CGLTF_INCLUDE_DIR)
    message(FATAL_ERROR "cgltf header not found. Ensure vcpkg cgltf package is installed.")
endif()
if(NOT TARGET cgltf::cgltf)
    add_library(cgltf INTERFACE)
    add_library(cgltf::cgltf ALIAS cgltf)
    target_include_directories(cgltf INTERFACE ${CGLTF_INCLUDE_DIR})
endif()

# miniaudio (audio - header only)
find_path(MINIAUDIO_INCLUDE_DIR NAMES miniaudio.h)
if(NOT MINIAUDIO_INCLUDE_DIR)
    message(FATAL_ERROR "miniaudio header not found. Ensure vcpkg miniaudio package is installed.")
endif()
if(NOT TARGET miniaudio::miniaudio)
    add_library(miniaudio INTERFACE)
    add_library(miniaudio::miniaudio ALIAS miniaudio)
    target_include_directories(miniaudio INTERFACE ${MINIAUDIO_INCLUDE_DIR})
endif()

# dr_libs (audio format parsing - header only: dr_wav, dr_mp3, dr_flac)
find_path(DR_LIBS_INCLUDE_DIR NAMES dr_wav.h dr_mp3.h dr_flac.h)
if(NOT DR_LIBS_INCLUDE_DIR)
    message(FATAL_ERROR "dr_libs headers not found. Ensure vcpkg drlibs package is installed.")
endif()
if(NOT TARGET dr_libs::dr_libs)
    add_library(dr_libs INTERFACE)
    add_library(dr_libs::dr_libs ALIAS dr_libs)
    target_include_directories(dr_libs INTERFACE ${DR_LIBS_INCLUDE_DIR})
endif()

# tinyobjloader (OBJ file parsing - header only)
find_package(tinyobjloader CONFIG REQUIRED)
# vcpkg provides tinyobjloader::tinyobjloader - matches our usage

# ============================================================================
# ufbx (FBX file parsing - single file, not in vcpkg)
# ============================================================================
FetchContent_Declare(
    ufbx
    GIT_REPOSITORY https://github.com/bqqbarbhg/ufbx.git
    GIT_TAG v0.14.3
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(ufbx)
if(NOT ufbx_POPULATED)
    FetchContent_Populate(ufbx)
endif()
add_library(ufbx STATIC ${ufbx_SOURCE_DIR}/ufbx.c)
add_library(ufbx::ufbx ALIAS ufbx)
target_include_directories(ufbx PUBLIC ${ufbx_SOURCE_DIR})
target_compile_definitions(ufbx PUBLIC UFBX_REAL_IS_FLOAT)

# ============================================================================
# Jolt Physics
# ============================================================================
find_package(Jolt CONFIG REQUIRED)
# vcpkg provides Jolt::Jolt - physics/CMakeLists.txt updated to use this

# ============================================================================
# Recast Navigation (pathfinding)
# ============================================================================
find_package(recastnavigation CONFIG REQUIRED)
# vcpkg provides RecastNavigation::Recast etc.
# Our code uses Recast::Recast, Recast::Detour, etc. - create aliases
if(NOT TARGET Recast::Recast)
    add_library(Recast::Recast ALIAS RecastNavigation::Recast)
endif()
if(NOT TARGET Recast::Detour)
    add_library(Recast::Detour ALIAS RecastNavigation::Detour)
endif()
if(NOT TARGET Recast::DetourCrowd)
    add_library(Recast::DetourCrowd ALIAS RecastNavigation::DetourCrowd)
endif()
if(NOT TARGET Recast::DetourTileCache)
    add_library(Recast::DetourTileCache ALIAS RecastNavigation::DetourTileCache)
endif()

# ============================================================================
# bgfx.cmake (via FetchContent - vcpkg version is older and missing shader tools)
# ============================================================================
# Cache bgfx outside the build directory so clean builds don't re-download
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "FetchContent base directory")

FetchContent_Declare(
    bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG v1.135.9062-502
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
    UPDATE_DISCONNECTED TRUE
)

# Configure bgfx.cmake options
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_AMALGAMATED OFF CACHE BOOL "" FORCE)
set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)

# Enable bgfx debug features only in Debug builds
# Note: Only set for single-config generators where CMAKE_BUILD_TYPE is defined
if(CMAKE_BUILD_TYPE)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BGFX_CONFIG_DEBUG ON CACHE BOOL "" FORCE)
    else()
        set(BGFX_CONFIG_DEBUG OFF CACHE BOOL "" FORCE)
    endif()
endif()

FetchContent_MakeAvailable(bgfx_cmake)

# Restore FETCHCONTENT_BASE_DIR to default for any future FetchContent usage
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "FetchContent base directory" FORCE)

# Fix broken imstb_textedit.h wrapper in bgfx's dear-imgui bundle
# The bundled imstb_textedit.h is a 47-line wrapper that incorrectly bridges to stb_textedit.h,
# but imgui_widgets.cpp expects the full 1470-line DearImGui-modified version.
# The correct file is already present in bgfx's stb directory, so we copy it over.
if(EXISTS "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/stb/stb_textedit.h")
    file(COPY_FILE
        "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/stb/stb_textedit.h"
        "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/dear-imgui/imstb_textedit.h"
        ONLY_IF_DIFFERENT
    )
    message(STATUS "Patched bgfx dear-imgui imstb_textedit.h")
endif()

if(EXISTS "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/stb/stb_rect_pack.h")
    file(COPY_FILE
        "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/stb/stb_rect_pack.h"
        "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/dear-imgui/imstb_rectpack.h"
        ONLY_IF_DIFFERENT
    )
    message(STATUS "Patched bgfx dear-imgui imstb_rectpack.h")
endif()

if(EXISTS "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/stb/stb_truetype.h")
    file(COPY_FILE
        "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/stb/stb_truetype.h"
        "${bgfx_cmake_SOURCE_DIR}/bgfx/3rdparty/dear-imgui/imstb_truetype.h"
        ONLY_IF_DIFFERENT
    )
    message(STATUS "Patched bgfx dear-imgui imstb_truetype.h")
endif()

# Create namespace aliases
add_library(bgfx::bgfx ALIAS bgfx)
add_library(bgfx::bx ALIAS bx)
add_library(bgfx::bimg ALIAS bimg)

# bgfx.cmake creates raw targets (bgfx, bx, bimg).
# - bgfx::bgfx      (main library)
# - bgfx::bx        (base library)
# - bgfx::bimg      (image library)
# Tools: shaderc, geometryc, texturec

# ============================================================================
# EnTT Entity Component System
# ============================================================================
find_package(EnTT CONFIG REQUIRED)
# vcpkg provides EnTT::EnTT - our code uses entt::entt (lowercase)
if(NOT TARGET entt::entt)
    add_library(entt::entt ALIAS EnTT::EnTT)
endif()

# ============================================================================
# Lua (scripting runtime)
# ============================================================================
find_package(lua CONFIG REQUIRED)
# vcpkg's lua with "cpp" feature compiles as C++ - provides lua target

# ============================================================================
# sol2 (C++ Lua wrapper - header only)
# ============================================================================
find_package(sol2 CONFIG REQUIRED)
# Add required compile definitions for safety and C++ Lua compatibility
if(TARGET sol2::sol2)
    set_property(TARGET sol2::sol2 APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS SOL_ALL_SAFETIES_ON=1 SOL_USING_CXX_LUA=1)
elseif(TARGET sol2)
    set_property(TARGET sol2 APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS SOL_ALL_SAFETIES_ON=1 SOL_USING_CXX_LUA=1)
endif()

# ============================================================================
# Catch2 (unit testing framework)
# ============================================================================
if(ENGINE_BUILD_TESTS)
    find_package(Catch2 3 CONFIG REQUIRED)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_DIR})
endif()

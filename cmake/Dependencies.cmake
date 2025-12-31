# Dependencies.cmake
# External dependencies including bgfx.cmake

include(FetchContent)

# ============================================================================
# GLM (math library)
# ============================================================================
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# ============================================================================
# nlohmann/json (serialization)
# ============================================================================
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# ============================================================================
# stb (image loading - header only)
# ============================================================================
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()
add_library(stb INTERFACE)
add_library(stb::stb ALIAS stb)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# ============================================================================
# cgltf (glTF loading - header only)
# ============================================================================
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.14
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(cgltf)
if(NOT cgltf_POPULATED)
    FetchContent_Populate(cgltf)
endif()
add_library(cgltf INTERFACE)
add_library(cgltf::cgltf ALIAS cgltf)
target_include_directories(cgltf INTERFACE ${cgltf_SOURCE_DIR})

# ============================================================================
# miniaudio (audio - header only)
# ============================================================================
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()
add_library(miniaudio INTERFACE)
add_library(miniaudio::miniaudio ALIAS miniaudio)
target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR}/extras/miniaudio_split)

# ============================================================================
# Jolt Physics
# ============================================================================
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.2.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build
    UPDATE_DISCONNECTED TRUE
)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)  # Enable exceptions to match rest of project
FetchContent_MakeAvailable(JoltPhysics)

# ============================================================================
# bgfx.cmake
# ============================================================================
# Configure bgfx.cmake options
set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_AMALGAMATED OFF CACHE BOOL "" FORCE)

# Enable bgfx debug features only in Debug builds
# Note: Only set for single-config generators where CMAKE_BUILD_TYPE is defined
if(CMAKE_BUILD_TYPE)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(BGFX_CONFIG_DEBUG ON CACHE BOOL "" FORCE)
    else()
        set(BGFX_CONFIG_DEBUG OFF CACHE BOOL "" FORCE)
    endif()
endif()

# Add bgfx.cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/external/bgfx.cmake)

# Create namespace aliases
add_library(bgfx::bgfx ALIAS bgfx)
add_library(bgfx::bx ALIAS bx)
add_library(bgfx::bimg ALIAS bimg)

# bgfx.cmake creates raw targets (bgfx, bx, bimg).
# - bgfx::bgfx      (main library)
# - bgfx::bx        (base library)
# - bgfx::bimg      (image library)
# Tools: shaderc, geometryc, texturec

# EnTT Entity Component System (header-only)
add_library(entt INTERFACE)
add_library(entt::entt ALIAS entt)
target_include_directories(entt INTERFACE
    ${CMAKE_SOURCE_DIR}/external/entt/single_include
)

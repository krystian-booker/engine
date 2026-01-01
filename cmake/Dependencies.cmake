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
# Recast Navigation (pathfinding)
# ============================================================================
FetchContent_Declare(
    recastnavigation
    GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation.git
    GIT_TAG v1.6.0
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(recastnavigation)

# Create namespace aliases for Recast/Detour
add_library(Recast::Recast ALIAS Recast)
add_library(Recast::Detour ALIAS Detour)
add_library(Recast::DetourCrowd ALIAS DetourCrowd)
add_library(Recast::DetourTileCache ALIAS DetourTileCache)

# ============================================================================
# bgfx.cmake (via FetchContent)
# ============================================================================
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
# EnTT Entity Component System (via FetchContent)
# ============================================================================
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.16.0
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(entt)

# ============================================================================
# Lua (scripting runtime)
# ============================================================================
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.7
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
endif()

# Build Lua as a static library
set(LUA_SOURCES
    ${lua_SOURCE_DIR}/lapi.c
    ${lua_SOURCE_DIR}/lauxlib.c
    ${lua_SOURCE_DIR}/lbaselib.c
    ${lua_SOURCE_DIR}/lcode.c
    ${lua_SOURCE_DIR}/lcorolib.c
    ${lua_SOURCE_DIR}/lctype.c
    ${lua_SOURCE_DIR}/ldblib.c
    ${lua_SOURCE_DIR}/ldebug.c
    ${lua_SOURCE_DIR}/ldo.c
    ${lua_SOURCE_DIR}/ldump.c
    ${lua_SOURCE_DIR}/lfunc.c
    ${lua_SOURCE_DIR}/lgc.c
    ${lua_SOURCE_DIR}/linit.c
    ${lua_SOURCE_DIR}/liolib.c
    ${lua_SOURCE_DIR}/llex.c
    ${lua_SOURCE_DIR}/lmathlib.c
    ${lua_SOURCE_DIR}/lmem.c
    ${lua_SOURCE_DIR}/loadlib.c
    ${lua_SOURCE_DIR}/lobject.c
    ${lua_SOURCE_DIR}/lopcodes.c
    ${lua_SOURCE_DIR}/loslib.c
    ${lua_SOURCE_DIR}/lparser.c
    ${lua_SOURCE_DIR}/lstate.c
    ${lua_SOURCE_DIR}/lstring.c
    ${lua_SOURCE_DIR}/lstrlib.c
    ${lua_SOURCE_DIR}/ltable.c
    ${lua_SOURCE_DIR}/ltablib.c
    ${lua_SOURCE_DIR}/ltm.c
    ${lua_SOURCE_DIR}/lundump.c
    ${lua_SOURCE_DIR}/lutf8lib.c
    ${lua_SOURCE_DIR}/lvm.c
    ${lua_SOURCE_DIR}/lzio.c
)
add_library(lua STATIC ${LUA_SOURCES})
add_library(lua::lua ALIAS lua)
target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})
# Compile as C++
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)
target_compile_definitions(lua PRIVATE LUA_USE_WINDOWS)

# ============================================================================
# sol2 (C++ Lua wrapper - header only)
# ============================================================================
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.1
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_GetProperties(sol2)
if(NOT sol2_POPULATED)
    FetchContent_Populate(sol2)
endif()
add_library(sol2 INTERFACE)
add_library(sol2::sol2 ALIAS sol2)
target_include_directories(sol2 INTERFACE ${sol2_SOURCE_DIR}/include)
target_link_libraries(sol2 INTERFACE lua::lua)
target_compile_definitions(sol2 INTERFACE
    SOL_ALL_SAFETIES_ON=1
    SOL_USING_CXX_LUA=1
)

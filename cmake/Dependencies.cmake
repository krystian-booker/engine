# Dependencies.cmake
# External dependencies including bgfx.cmake

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

# Dependencies.cmake
# External dependencies including bgfx.cmake

# Configure bgfx.cmake options
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_AMALGAMATED OFF CACHE BOOL "" FORCE)

# Enable bgfx debug features only in Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BGFX_CONFIG_DEBUG ON CACHE BOOL "" FORCE)
else()
    set(BGFX_CONFIG_DEBUG OFF CACHE BOOL "" FORCE)
endif()

# Add bgfx.cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/external/bgfx.cmake)

# bgfx.cmake creates these targets:
# - bgfx::bgfx      (main library)
# - bgfx::bx        (base library)
# - bgfx::bimg      (image library)
# - shaderc, geometryc, texturec (tools)

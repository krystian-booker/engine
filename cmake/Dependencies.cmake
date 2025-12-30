# Dependencies.cmake
# External dependencies including bgfx.cmake

# Configure bgfx.cmake options
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_AMALGAMATED OFF CACHE BOOL "" FORCE)
set(BGFX_CONFIG_DEBUG ON CACHE BOOL "" FORCE)

# Point to local bgfx submodules
set(BGFX_DIR ${CMAKE_SOURCE_DIR}/external/bgfx CACHE STRING "" FORCE)
set(BX_DIR ${CMAKE_SOURCE_DIR}/external/bx CACHE STRING "" FORCE)
set(BIMG_DIR ${CMAKE_SOURCE_DIR}/external/bimg CACHE STRING "" FORCE)

# Add bgfx.cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/external/bgfx.cmake)

# bgfx.cmake creates these targets:
# - bgfx::bgfx      (main library)
# - bgfx::bx        (base library)
# - bgfx::bimg      (image library)
# - shaderc, geometryc, texturec (tools)

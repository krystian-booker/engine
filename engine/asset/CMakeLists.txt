# engine/asset/CMakeLists.txt
# engine::asset - asset management, importers, hot reload

add_library(engine_asset STATIC)
add_library(engine::asset ALIAS engine_asset)

target_sources(engine_asset
    PRIVATE
        src/importer.cpp
        src/hot_reload.cpp
        src/manager.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/engine/asset/importer.hpp
            include/engine/asset/hot_reload.hpp
            include/engine/asset/asset.hpp
            include/engine/asset/types.hpp
            include/engine/asset/manager.hpp
)

target_include_directories(engine_asset
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_asset
    PUBLIC
        engine::core
        engine::render
        stb::stb
        cgltf::cgltf
    PRIVATE
        engine_compiler_flags
        bgfx::bimg
)

set_target_properties(engine_asset PROPERTIES
    FOLDER "engine"
)

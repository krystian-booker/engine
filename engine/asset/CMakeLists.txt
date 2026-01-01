# engine/asset/CMakeLists.txt
# engine::asset - asset management, importers, hot reload

add_library(engine_asset STATIC)
add_library(engine::asset ALIAS engine_asset)

target_sources(engine_asset
    PRIVATE
        src/importer.cpp
        src/hot_reload.cpp
        src/manager.cpp
        src/gltf_importer.cpp
        src/obj_importer.cpp
        src/fbx_importer.cpp
        src/audio_loader.cpp
        src/material_loader.cpp
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
            include/engine/asset/gltf_importer.hpp
            include/engine/asset/obj_importer.hpp
            include/engine/asset/fbx_importer.hpp
            include/engine/asset/audio_loader.hpp
            include/engine/asset/material_loader.hpp
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
        nlohmann_json::nlohmann_json
    PRIVATE
        engine_compiler_flags
        bgfx::bimg
        tinyobjloader::tinyobjloader
        ufbx::ufbx
        dr_libs::dr_libs
)

set_target_properties(engine_asset PROPERTIES
    FOLDER "engine"
)

#pragma once

#include <engine/asset/types.hpp>
#include <string>
#include <memory>

namespace engine::asset {

// Generic importer interface
// Note: For most use cases, prefer using AssetManager directly which provides:
// - Caching and reference counting
// - Hot reload support
// - Typed accessors (load_mesh, load_texture, etc.)
//
// This interface is provided for cases where you need one-off imports
// without caching, or for custom asset processing pipelines.
struct Importer {
    // Import an asset based on file extension
    // Returns nullptr if the format is not supported
    static std::unique_ptr<Asset> import(const std::string& path);

    // Get the asset type that would be imported for a given path
    // Returns empty string if format is not recognized
    static std::string get_asset_type(const std::string& path);
};

} // namespace engine::asset

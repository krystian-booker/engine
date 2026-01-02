#pragma once

#include <engine/asset/types.hpp>
#include <memory>
#include <string>

namespace engine::asset {

// Prefab loader - loads JSON prefab files into PrefabAsset
class PrefabLoader {
public:
    // Load a prefab from a JSON file
    // Returns nullptr on failure
    static std::shared_ptr<PrefabAsset> load(const std::string& path);

    // Get the last error message
    static const std::string& get_last_error();

private:
    static std::string s_last_error;
};

} // namespace engine::asset

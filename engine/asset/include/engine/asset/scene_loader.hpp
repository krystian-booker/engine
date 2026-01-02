#pragma once

#include <engine/asset/types.hpp>
#include <memory>
#include <string>

namespace engine::asset {

// Scene loader - loads JSON scene files into SceneAsset
class SceneLoader {
public:
    // Load a scene from a JSON file
    // Returns nullptr on failure
    static std::shared_ptr<SceneAsset> load(const std::string& path);

    // Get the last error message
    static const std::string& get_last_error();

private:
    static std::string s_last_error;
};

} // namespace engine::asset

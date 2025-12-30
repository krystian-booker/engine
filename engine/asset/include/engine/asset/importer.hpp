#pragma once

#include <string>
#include <memory>

namespace engine::asset {

struct Asset {
    virtual ~Asset() = default;
};

struct Importer {
    static std::unique_ptr<Asset> import(const std::string& path);
};

} // namespace engine::asset

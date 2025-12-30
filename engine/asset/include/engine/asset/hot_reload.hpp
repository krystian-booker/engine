#pragma once

#include <string>
#include <functional>

namespace engine::asset {

using ReloadCallback = std::function<void(const std::string& path)>;

struct HotReload {
    static void init();
    static void shutdown();
    static void watch(const std::string& path, ReloadCallback callback);
    static void unwatch(const std::string& path);
    static void poll();
};

} // namespace engine::asset

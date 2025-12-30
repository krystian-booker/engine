#include <engine/asset/hot_reload.hpp>

namespace engine::asset {

void HotReload::init() {
    // TODO: implement file watcher
}

void HotReload::shutdown() {
    // TODO: implement
}

void HotReload::watch(const std::string& /*path*/, ReloadCallback /*callback*/) {
    // TODO: implement
}

void HotReload::unwatch(const std::string& /*path*/) {
    // TODO: implement
}

void HotReload::poll() {
    // TODO: implement
}

} // namespace engine::asset

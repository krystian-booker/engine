#pragma once

// Umbrella header for engine::plugin module

#include <engine/plugin/game_interface.hpp>
#include <engine/plugin/plugin_loader.hpp>
#include <engine/plugin/system_registry.hpp>
#include <engine/plugin/hot_reload_manager.hpp>

namespace engine::plugin {

// Module version
constexpr int PLUGIN_VERSION_MAJOR = 1;
constexpr int PLUGIN_VERSION_MINOR = 0;

} // namespace engine::plugin

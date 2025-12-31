#pragma once

#include <entt/entt.hpp>
#include <cstdint>

namespace engine::scene {

// Entity is just a type alias for entt::entity
using Entity = entt::entity;

// Null entity constant
constexpr Entity NullEntity = entt::null;

// Entity info component for debugging/editor
struct EntityInfo {
    std::string name;
    uint64_t uuid = 0;
    bool enabled = true;
};

} // namespace engine::scene

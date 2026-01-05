#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include <functional>

namespace engine::reflect {

// Context for resolving entity <-> UUID during serialization/deserialization.
// Entity IDs are transient and change between sessions, so entity references
// must be serialized as UUIDs and resolved during deserialization.
struct EntityResolutionContext {
    // For serialization: convert entity to its UUID (from EntityInfo component)
    std::function<uint64_t(entt::entity)> entity_to_uuid;

    // For deserialization: convert UUID back to entity
    std::function<entt::entity(uint64_t)> uuid_to_entity;

    // Null entity UUID (0 = no entity / null reference)
    static constexpr uint64_t NullUUID = 0;

    // Check if context is valid for serialization
    bool can_serialize() const { return entity_to_uuid != nullptr; }

    // Check if context is valid for deserialization
    bool can_deserialize() const { return uuid_to_entity != nullptr; }
};

} // namespace engine::reflect

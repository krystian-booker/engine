#pragma once

#include <engine/core/math.hpp>
#include <cstdint>
#include <string>

namespace engine::core {

// ============================================================================
// Window Events
// ============================================================================

struct WindowResizeEvent {
    uint32_t width;
    uint32_t height;
};

struct WindowCloseEvent {};

struct WindowFocusEvent {
    bool focused;
};

// ============================================================================
// Input Events (low-level, prefer Input system for gameplay)
// ============================================================================

struct KeyEvent {
    int key;
    bool pressed;
    bool repeat;
};

struct MouseButtonEvent {
    int button;
    bool pressed;
};

struct MouseMoveEvent {
    float x;
    float y;
    float delta_x;
    float delta_y;
};

struct MouseScrollEvent {
    float x_offset;
    float y_offset;
};

// ============================================================================
// Asset Events
// ============================================================================

struct AssetLoadedEvent {
    std::string path;
    std::string type;  // "mesh", "texture", "material", etc.
};

struct AssetReloadedEvent {
    std::string path;
    std::string type;
};

struct AssetUnloadedEvent {
    std::string path;
};

// ============================================================================
// Scene Events
// ============================================================================

struct SceneLoadedEvent {
    std::string path;
};

struct SceneUnloadedEvent {
    std::string path;
};

// ============================================================================
// Entity Events (dispatched by scene::World)
// Uses uint32_t for entity ID to avoid circular dependency with entt
// ============================================================================

struct EntityCreatedEvent {
    uint32_t entity_id;  // Cast from entt::entity
};

struct EntityDestroyedEvent {
    uint32_t entity_id;  // Cast from entt::entity
};

struct ComponentAddedEvent {
    uint32_t entity_id;  // Cast from entt::entity
    size_t component_type_hash;  // typeid(T).hash_code()
};

struct ComponentRemovedEvent {
    uint32_t entity_id;  // Cast from entt::entity
    size_t component_type_hash;
};

struct HierarchyChangedEvent {
    uint32_t entity_id;
    uint32_t old_parent_id;  // UINT32_MAX = no parent
    uint32_t new_parent_id;  // UINT32_MAX = no parent
};

// ============================================================================
// Physics Events (dispatched by physics::PhysicsWorld)
// ============================================================================

struct CollisionStartEvent {
    uint32_t body_a_id;
    uint32_t body_b_id;
    Vec3 contact_point;
    Vec3 contact_normal;
    float penetration_depth;
};

struct CollisionEndEvent {
    uint32_t body_a_id;
    uint32_t body_b_id;
};

struct TriggerEnterEvent {
    uint32_t trigger_id;
    uint32_t other_id;
};

struct TriggerExitEvent {
    uint32_t trigger_id;
    uint32_t other_id;
};

// ============================================================================
// Audio Events
// ============================================================================

struct SoundFinishedEvent {
    uint32_t sound_id;
};

struct MusicFinishedEvent {
    uint32_t music_id;
};

} // namespace engine::core

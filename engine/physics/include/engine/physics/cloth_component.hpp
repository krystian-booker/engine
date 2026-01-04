#pragma once

#include <engine/physics/body.hpp>
#include <vector>
#include <cstdint>

namespace engine::physics {

// Cloth simulation type
enum class ClothType : uint8_t {
    Visual,         // Optimized for visuals only (capes, flags, drapes)
    Interactive     // Full collision with world and player
};

// Wind mode for cloth
enum class ClothWindMode : uint8_t {
    None,           // No wind effects
    Global,         // Use global wind settings
    Local,          // Per-cloth wind settings
    Turbulent       // Dynamic turbulent wind
};

// How a cloth vertex is attached
enum class AttachmentType : uint8_t {
    Fixed,          // Completely fixed position
    Sliding,        // Can slide along a path
    Spring          // Connected with spring constraint
};

// Grid generation parameters
struct ClothGridSettings {
    uint32_t width_segments = 10;     // Number of horizontal segments
    uint32_t height_segments = 10;    // Number of vertical segments
    float width = 2.0f;               // Total width in meters
    float height = 2.0f;              // Total height in meters
    bool double_sided = true;         // Generate both sides
};

// Mesh-based cloth settings
struct ClothMeshSettings {
    bool use_grid = true;             // True = procedural grid, false = custom mesh
    ClothGridSettings grid;           // Grid generation parameters

    // For custom mesh (when use_grid = false)
    std::vector<Vec3> vertices;       // Custom vertex positions
    std::vector<Vec3> normals;        // Custom normals
    std::vector<Vec2> uvs;            // UV coordinates
    std::vector<uint32_t> indices;    // Triangle indices
};

// Single cloth attachment point
struct ClothAttachment {
    uint32_t vertex_index = 0;        // Which vertex to attach
    AttachmentType type = AttachmentType::Fixed;

    // For entity-attached points
    bool attach_to_entity = false;
    uint32_t entity_id = 0;           // EnTT entity (if attached)
    Vec3 local_offset{0.0f};          // Offset from entity

    // For world-attached points
    Vec3 world_position{0.0f};        // Fixed world position

    // For spring attachments
    float spring_stiffness = 1000.0f;
    float spring_damping = 10.0f;
    float max_distance = 0.1f;        // Max stretch distance
};

// Collision settings for cloth
struct ClothCollisionSettings {
    bool self_collision = false;      // Cloth-cloth collision (expensive)
    bool world_collision = true;      // Collision with static geometry
    bool dynamic_collision = true;    // Collision with dynamic bodies
    float collision_margin = 0.02f;   // Distance to keep from objects
    uint16_t collision_mask = 0xFFFF; // Which layers to collide with
};

// Wind settings for cloth
struct ClothWindSettings {
    Vec3 direction{1.0f, 0.0f, 0.0f}; // Wind direction (normalized)
    float strength = 1.0f;            // Wind force strength
    float turbulence = 0.3f;          // Random variation (0-1)
    float turbulence_frequency = 2.0f; // How fast turbulence changes
    float drag_coefficient = 0.5f;    // Air resistance
};

// Main cloth component
struct ClothComponent {
    ClothType type = ClothType::Visual;
    ClothMeshSettings mesh;

    // Physical properties
    float mass = 1.0f;                // Total mass (distributed across vertices)
    float edge_stiffness = 0.8f;      // Stretch resistance (0-1, higher = stiffer)
    float bend_stiffness = 0.1f;      // Fold resistance (0-1, higher = stiffer)
    float shear_stiffness = 0.5f;     // Shear resistance (0-1)
    float damping = 0.1f;             // Motion damping

    // Solver settings
    int solver_iterations = 4;        // XPBD solver iterations (quality vs perf)
    float substep_delta = 1.0f / 120.0f; // Physics substep size

    // Attachments
    std::vector<ClothAttachment> attachments;

    // Collision
    ClothCollisionSettings collision;

    // Wind
    ClothWindMode wind_mode = ClothWindMode::Global;
    ClothWindSettings wind;

    // Visual-only optimization
    float visual_update_rate = 60.0f;    // Update rate for visual cloth
    float visual_max_distance = 50.0f;   // Distance at which to stop simulating

    // Gravity
    bool use_gravity = true;
    Vec3 custom_gravity{0.0f, -9.81f, 0.0f};

    // Sleep/wake
    float sleep_threshold = 0.01f;    // Velocity below which to sleep
    bool is_sleeping = false;         // Runtime: cloth is sleeping

    // Runtime state
    bool initialized = false;
};

// Cloth runtime state (for queries)
struct ClothState {
    // Vertex data (for rendering)
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec3> velocities;

    // Bounds
    Vec3 bounds_min{0.0f};
    Vec3 bounds_max{0.0f};
    Vec3 center{0.0f};

    // Status
    bool is_active = true;
    bool is_sleeping = false;
    float total_kinetic_energy = 0.0f;
    int active_vertices = 0;
};

// Factory functions for common cloth configurations
ClothComponent make_cape(float width = 1.0f, float height = 1.5f);
ClothComponent make_flag(float width = 2.0f, float height = 1.2f);
ClothComponent make_curtain(float width = 3.0f, float height = 2.5f);
ClothComponent make_banner(float width = 0.8f, float height = 2.0f);
ClothComponent make_tablecloth(float width = 2.0f, float height = 2.0f);
ClothComponent make_rope(float length = 5.0f, int segments = 20);

} // namespace engine::physics

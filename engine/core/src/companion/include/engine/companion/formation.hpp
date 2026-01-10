#pragma once

#include <engine/core/math.hpp>
#include <vector>
#include <cstdint>

namespace engine::companion {

using namespace engine::core;

// ============================================================================
// Formation Types
// ============================================================================

enum class FormationType : uint8_t {
    Line,           // Companions in a line behind leader
    Wedge,          // V-formation behind leader
    Circle,         // Surrounding the leader
    Column,         // Single file behind leader
    Spread,         // Spread out for combat
    Custom          // User-defined positions
};

const char* formation_type_to_string(FormationType type);

// ============================================================================
// Formation Slot
// ============================================================================

struct FormationSlot {
    Vec3 offset{0.0f};          // Offset from leader position (local space)
    float facing_offset = 0.0f;  // Facing angle offset from leader (radians)
    int priority = 0;           // Lower = filled first
    bool occupied = false;
};

// ============================================================================
// Formation
// ============================================================================

struct Formation {
    FormationType type = FormationType::Wedge;
    float spacing = 2.0f;                   // Base spacing between companions
    float row_offset = 1.5f;                // Offset between rows (for wedge/column)
    std::vector<FormationSlot> slots;

    // ========================================================================
    // Preset Generators
    // ========================================================================

    // Generate a wedge/V formation
    static Formation wedge(int max_companions, float spacing = 2.0f);

    // Generate a line formation (side by side)
    static Formation line(int max_companions, float spacing = 2.0f);

    // Generate a circle formation
    static Formation circle(int max_companions, float radius = 3.0f);

    // Generate a column formation (single file)
    static Formation column(int max_companions, float spacing = 2.0f);

    // Generate a spread formation (combat-ready)
    static Formation spread(int max_companions, float spacing = 3.0f);

    // ========================================================================
    // Slot Management
    // ========================================================================

    // Get the next available slot index, or -1 if none
    int get_next_available_slot() const;

    // Mark a slot as occupied/unoccupied
    void set_slot_occupied(int slot_index, bool occupied);

    // Get number of occupied slots
    int get_occupied_count() const;

    // Get total capacity
    int get_capacity() const { return static_cast<int>(slots.size()); }

    // Clear all occupancy
    void clear_occupancy();
};

// ============================================================================
// Formation Position Calculation
// ============================================================================

// Calculate world position for a formation slot
// leader_pos: World position of the leader
// leader_forward: Forward direction of the leader (normalized)
// slot: The formation slot to calculate for
Vec3 calculate_formation_position(
    const Vec3& leader_pos,
    const Vec3& leader_forward,
    const FormationSlot& slot
);

// Calculate world position for a slot index in a formation
Vec3 calculate_formation_position(
    const Formation& formation,
    int slot_index,
    const Vec3& leader_pos,
    const Vec3& leader_forward
);

// Calculate facing direction for a slot
Vec3 calculate_formation_facing(
    const Vec3& leader_forward,
    const FormationSlot& slot
);

// ============================================================================
// Formation Utilities
// ============================================================================

// Find the best slot for a new companion based on current positions
int find_best_slot(
    const Formation& formation,
    const Vec3& companion_pos,
    const Vec3& leader_pos,
    const Vec3& leader_forward
);

// Reassign slots to minimize total movement
void optimize_slot_assignments(
    Formation& formation,
    const std::vector<Vec3>& companion_positions,
    const Vec3& leader_pos,
    const Vec3& leader_forward
);

} // namespace engine::companion

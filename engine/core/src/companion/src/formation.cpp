#include <engine/companion/formation.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

namespace engine::companion {

const char* formation_type_to_string(FormationType type) {
    switch (type) {
        case FormationType::Line:    return "Line";
        case FormationType::Wedge:   return "Wedge";
        case FormationType::Circle:  return "Circle";
        case FormationType::Column:  return "Column";
        case FormationType::Spread:  return "Spread";
        case FormationType::Custom:  return "Custom";
        default:                     return "Unknown";
    }
}

// ============================================================================
// Formation Preset Generators
// ============================================================================

Formation Formation::wedge(int max_companions, float spacing) {
    Formation f;
    f.type = FormationType::Wedge;
    f.spacing = spacing;
    f.row_offset = spacing * 0.8f;
    f.slots.reserve(max_companions);

    // Generate wedge positions
    // Row 0: empty (leader)
    // Row 1: 2 positions (left, right)
    // Row 2: 2 positions (further left, further right)
    // etc.

    int slot_index = 0;
    int row = 1;

    while (slot_index < max_companions) {
        float row_z = -f.row_offset * row;

        // Left side
        if (slot_index < max_companions) {
            FormationSlot slot;
            slot.offset = Vec3(-spacing * row * 0.5f, 0.0f, row_z);
            slot.facing_offset = 0.0f;
            slot.priority = slot_index;
            f.slots.push_back(slot);
            slot_index++;
        }

        // Right side
        if (slot_index < max_companions) {
            FormationSlot slot;
            slot.offset = Vec3(spacing * row * 0.5f, 0.0f, row_z);
            slot.facing_offset = 0.0f;
            slot.priority = slot_index;
            f.slots.push_back(slot);
            slot_index++;
        }

        row++;
    }

    return f;
}

Formation Formation::line(int max_companions, float spacing) {
    Formation f;
    f.type = FormationType::Line;
    f.spacing = spacing;
    f.slots.reserve(max_companions);

    // Generate line positions (side by side, behind leader)
    float total_width = (max_companions - 1) * spacing;
    float start_x = -total_width / 2.0f;

    for (int i = 0; i < max_companions; ++i) {
        FormationSlot slot;
        slot.offset = Vec3(start_x + i * spacing, 0.0f, -spacing);
        slot.facing_offset = 0.0f;
        slot.priority = std::abs(i - max_companions / 2);  // Center slots have priority
        f.slots.push_back(slot);
    }

    return f;
}

Formation Formation::circle(int max_companions, float radius) {
    Formation f;
    f.type = FormationType::Circle;
    f.spacing = radius;
    f.slots.reserve(max_companions);

    // Generate positions in a circle around the leader
    float angle_step = 2.0f * 3.14159265f / static_cast<float>(max_companions);

    // Start from behind (180 degrees) and go around
    float start_angle = 3.14159265f;

    for (int i = 0; i < max_companions; ++i) {
        float angle = start_angle + i * angle_step;
        FormationSlot slot;
        slot.offset = Vec3(
            std::sin(angle) * radius,
            0.0f,
            std::cos(angle) * radius
        );
        slot.facing_offset = angle + 3.14159265f;  // Face inward toward leader
        slot.priority = i;
        f.slots.push_back(slot);
    }

    return f;
}

Formation Formation::column(int max_companions, float spacing) {
    Formation f;
    f.type = FormationType::Column;
    f.spacing = spacing;
    f.slots.reserve(max_companions);

    // Generate positions in a single file behind leader
    for (int i = 0; i < max_companions; ++i) {
        FormationSlot slot;
        slot.offset = Vec3(0.0f, 0.0f, -(i + 1) * spacing);
        slot.facing_offset = 0.0f;
        slot.priority = i;
        f.slots.push_back(slot);
    }

    return f;
}

Formation Formation::spread(int max_companions, float spacing) {
    Formation f;
    f.type = FormationType::Spread;
    f.spacing = spacing;
    f.slots.reserve(max_companions);

    // Generate spread positions (wider than line, staggered)
    int rows = (max_companions + 2) / 3;  // Roughly 3 per row

    int slot_index = 0;
    for (int row = 0; row < rows && slot_index < max_companions; ++row) {
        int in_row = std::min(3, max_companions - slot_index);
        float row_width = (in_row - 1) * spacing;
        float start_x = -row_width / 2.0f;
        float row_z = -(row + 1) * spacing * 0.8f;

        for (int i = 0; i < in_row && slot_index < max_companions; ++i) {
            FormationSlot slot;
            slot.offset = Vec3(start_x + i * spacing, 0.0f, row_z);
            slot.facing_offset = 0.0f;
            slot.priority = slot_index;
            f.slots.push_back(slot);
            slot_index++;
        }
    }

    return f;
}

// ============================================================================
// Slot Management
// ============================================================================

int Formation::get_next_available_slot() const {
    // Find first unoccupied slot, respecting priority
    int best_slot = -1;
    int best_priority = std::numeric_limits<int>::max();

    for (size_t i = 0; i < slots.size(); ++i) {
        if (!slots[i].occupied && slots[i].priority < best_priority) {
            best_slot = static_cast<int>(i);
            best_priority = slots[i].priority;
        }
    }

    return best_slot;
}

void Formation::set_slot_occupied(int slot_index, bool occupied) {
    if (slot_index >= 0 && slot_index < static_cast<int>(slots.size())) {
        slots[slot_index].occupied = occupied;
    }
}

int Formation::get_occupied_count() const {
    int count = 0;
    for (const auto& slot : slots) {
        if (slot.occupied) {
            count++;
        }
    }
    return count;
}

void Formation::clear_occupancy() {
    for (auto& slot : slots) {
        slot.occupied = false;
    }
}

// ============================================================================
// Position Calculation
// ============================================================================

Vec3 calculate_formation_position(
    const Vec3& leader_pos,
    const Vec3& leader_forward,
    const FormationSlot& slot
) {
    // Create rotation matrix from forward direction
    Vec3 forward = glm::normalize(Vec3(leader_forward.x, 0.0f, leader_forward.z));
    if (glm::length(forward) < 0.01f) {
        forward = Vec3(0.0f, 0.0f, 1.0f);
    }

    Vec3 right = glm::normalize(glm::cross(Vec3(0.0f, 1.0f, 0.0f), forward));
    Vec3 up = Vec3(0.0f, 1.0f, 0.0f);

    // Transform local offset to world space
    Vec3 world_offset = right * slot.offset.x + up * slot.offset.y + forward * slot.offset.z;

    return leader_pos + world_offset;
}

Vec3 calculate_formation_position(
    const Formation& formation,
    int slot_index,
    const Vec3& leader_pos,
    const Vec3& leader_forward
) {
    if (slot_index < 0 || slot_index >= static_cast<int>(formation.slots.size())) {
        return leader_pos;  // Invalid slot, return leader position
    }

    return calculate_formation_position(leader_pos, leader_forward, formation.slots[slot_index]);
}

Vec3 calculate_formation_facing(
    const Vec3& leader_forward,
    const FormationSlot& slot
) {
    if (std::abs(slot.facing_offset) < 0.001f) {
        return leader_forward;
    }

    // Rotate forward by facing offset around Y axis
    float cos_a = std::cos(slot.facing_offset);
    float sin_a = std::sin(slot.facing_offset);

    return Vec3(
        leader_forward.x * cos_a - leader_forward.z * sin_a,
        0.0f,
        leader_forward.x * sin_a + leader_forward.z * cos_a
    );
}

// ============================================================================
// Formation Utilities
// ============================================================================

int find_best_slot(
    const Formation& formation,
    const Vec3& companion_pos,
    const Vec3& leader_pos,
    const Vec3& leader_forward
) {
    int best_slot = -1;
    float best_distance = std::numeric_limits<float>::max();

    for (size_t i = 0; i < formation.slots.size(); ++i) {
        if (formation.slots[i].occupied) {
            continue;
        }

        Vec3 slot_pos = calculate_formation_position(
            leader_pos, leader_forward, formation.slots[i]);

        float dist = glm::distance(companion_pos, slot_pos);

        if (dist < best_distance) {
            best_distance = dist;
            best_slot = static_cast<int>(i);
        }
    }

    return best_slot;
}

void optimize_slot_assignments(
    Formation& formation,
    const std::vector<Vec3>& companion_positions,
    const Vec3& leader_pos,
    const Vec3& leader_forward
) {
    // Simple greedy assignment - assign closest companion to closest slot
    // More sophisticated approaches (Hungarian algorithm) could be used for larger parties

    formation.clear_occupancy();

    std::vector<bool> assigned(companion_positions.size(), false);

    for (size_t slot_idx = 0; slot_idx < formation.slots.size() && slot_idx < companion_positions.size(); ++slot_idx) {
        Vec3 slot_pos = calculate_formation_position(
            leader_pos, leader_forward, formation.slots[slot_idx]);

        int best_companion = -1;
        float best_distance = std::numeric_limits<float>::max();

        for (size_t comp_idx = 0; comp_idx < companion_positions.size(); ++comp_idx) {
            if (assigned[comp_idx]) continue;

            float dist = glm::distance(companion_positions[comp_idx], slot_pos);
            if (dist < best_distance) {
                best_distance = dist;
                best_companion = static_cast<int>(comp_idx);
            }
        }

        if (best_companion >= 0) {
            assigned[best_companion] = true;
            formation.slots[slot_idx].occupied = true;
        }
    }
}

} // namespace engine::companion

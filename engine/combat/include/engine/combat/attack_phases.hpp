#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace engine::combat {

// Attack phase states
enum class AttackPhase : uint8_t {
    None,           // Not attacking
    Startup,        // Wind-up/anticipation, can often be canceled
    Active,         // Hitbox active, dealing damage
    Recovery,       // Cool-down, typically vulnerable
    Canceled        // Attack was interrupted
};

// Attack definition (loaded from data or defined in code)
struct AttackDefinition {
    std::string name;

    // Phase durations (seconds)
    float startup_duration = 0.1f;
    float active_duration = 0.2f;
    float recovery_duration = 0.3f;

    // Cancel windows (normalized time 0-1 within recovery phase)
    bool can_cancel_startup = true;             // Can cancel during startup
    bool can_cancel_into_dodge = true;          // Can dodge during recovery
    bool can_cancel_into_attack = false;        // Can chain into next attack
    float cancel_window_start = 0.5f;           // When cancel window opens
    float cancel_window_end = 0.9f;             // When cancel window closes

    // Combo info
    std::string next_combo_attack;              // Attack to chain into
    int combo_position = 0;                     // Position in combo chain (0 = starter)
    int max_combo_chain = 3;                    // Max attacks in this combo

    // Movement during attack
    float forward_movement = 0.0f;              // Units to move forward
    bool root_motion = false;                   // Use animation root motion
    bool can_rotate = false;                    // Can adjust facing during attack

    // Associated hitboxes to activate
    std::vector<std::string> hitbox_ids;        // Hitboxes to activate during active phase

    // Animation
    std::string animation_name;
    float animation_speed = 1.0f;
};

// Attack phase component
struct AttackPhaseComponent {
    // Current state
    AttackPhase current_phase = AttackPhase::None;
    float phase_time = 0.0f;                    // Time in current phase
    float phase_duration = 0.0f;                // Duration of current phase

    // Current attack info
    std::string current_attack;
    AttackDefinition attack_def;                // Copy of current attack definition

    // Combo tracking
    int combo_count = 0;
    float combo_window_timer = 0.0f;            // Time left to continue combo
    float combo_window_duration = 0.5f;         // Window to input next attack
    std::string queued_attack;                  // Buffered next attack input

    // Hitstop tracking
    float hitstop_remaining = 0.0f;

    // State queries
    bool is_attacking() const {
        return current_phase != AttackPhase::None && current_phase != AttackPhase::Canceled;
    }

    bool is_in_startup() const { return current_phase == AttackPhase::Startup; }
    bool is_in_active() const { return current_phase == AttackPhase::Active; }
    bool is_in_recovery() const { return current_phase == AttackPhase::Recovery; }

    // Get normalized progress in current phase (0.0 to 1.0)
    float get_phase_progress() const {
        if (phase_duration <= 0.0f) return 1.0f;
        return phase_time / phase_duration;
    }

    // Get total attack progress (0.0 to 1.0 across all phases)
    float get_total_progress() const {
        if (!is_attacking()) return 0.0f;

        float total_duration = attack_def.startup_duration +
                               attack_def.active_duration +
                               attack_def.recovery_duration;
        if (total_duration <= 0.0f) return 1.0f;

        float elapsed = 0.0f;
        switch (current_phase) {
            case AttackPhase::Startup:
                elapsed = phase_time;
                break;
            case AttackPhase::Active:
                elapsed = attack_def.startup_duration + phase_time;
                break;
            case AttackPhase::Recovery:
                elapsed = attack_def.startup_duration + attack_def.active_duration + phase_time;
                break;
            default:
                break;
        }

        return elapsed / total_duration;
    }

    // Check if in cancel window
    bool can_cancel() const {
        if (current_phase == AttackPhase::Startup && attack_def.can_cancel_startup) {
            return true;
        }
        if (current_phase == AttackPhase::Recovery) {
            float progress = get_phase_progress();
            return progress >= attack_def.cancel_window_start &&
                   progress <= attack_def.cancel_window_end;
        }
        return false;
    }

    // Check if can chain into next combo attack
    bool can_combo() const {
        if (!attack_def.can_cancel_into_attack) return false;
        if (combo_count >= attack_def.max_combo_chain) return false;
        return can_cancel() || (current_phase == AttackPhase::Recovery && combo_window_timer > 0.0f);
    }

    // Queue a follow-up attack (for input buffering)
    void queue_attack(const std::string& attack_name) {
        queued_attack = attack_name;
    }

    // Clear the attack state
    void clear() {
        current_phase = AttackPhase::None;
        phase_time = 0.0f;
        phase_duration = 0.0f;
        current_attack.clear();
        queued_attack.clear();
        attack_def = AttackDefinition{};
    }
};

// Callbacks for attack phase events
using PhaseCallback = std::function<void(scene::Entity, AttackPhase, AttackPhase)>;
using HitCallback = std::function<void(scene::Entity attacker, scene::Entity target)>;

// Attack phase management system
class AttackPhaseManager {
public:
    static AttackPhaseManager& instance();

    // Delete copy/move
    AttackPhaseManager(const AttackPhaseManager&) = delete;
    AttackPhaseManager& operator=(const AttackPhaseManager&) = delete;

    // Start an attack on an entity
    bool start_attack(scene::World& world, scene::Entity entity, const std::string& attack_name);
    bool start_attack(scene::World& world, scene::Entity entity, const AttackDefinition& attack);

    // Cancel current attack
    void cancel_attack(scene::World& world, scene::Entity entity);

    // Process attack input (handles buffering and combo chaining)
    void process_attack_input(scene::World& world, scene::Entity entity, const std::string& attack_name);

    // Register attack definitions
    void register_attack(const AttackDefinition& attack);
    void register_attacks(const std::vector<AttackDefinition>& attacks);
    const AttackDefinition* get_attack(const std::string& name) const;

    // Callbacks
    void set_on_phase_changed(PhaseCallback callback) { m_on_phase_changed = callback; }
    void set_on_attack_hit(HitCallback callback) { m_on_attack_hit = callback; }

    // Get registered attack names
    std::vector<std::string> get_registered_attacks() const;

    void advance_phase(scene::World& world, scene::Entity entity, AttackPhaseComponent& attack);
    void activate_hitboxes(scene::World& world, scene::Entity entity, const AttackDefinition& attack);
    void deactivate_hitboxes(scene::World& world, scene::Entity entity, const AttackDefinition& attack);


private:
    AttackPhaseManager();
    ~AttackPhaseManager() = default;



    std::unordered_map<std::string, AttackDefinition> m_attacks;
    PhaseCallback m_on_phase_changed;
    HitCallback m_on_attack_hit;
};

// Convenience accessor
inline AttackPhaseManager& attacks() {
    return AttackPhaseManager::instance();
}

} // namespace engine::combat

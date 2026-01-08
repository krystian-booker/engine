#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/math.hpp>
#include <engine/combat/damage.hpp>
#include <string>

namespace engine::combat {

// ============================================================================
// Hit Reaction Types
// ============================================================================

enum class HitReactionType : uint8_t {
    None,
    Light,      // Quick flinch, doesn't interrupt action
    Medium,     // Visible reaction, brief stun
    Heavy,      // Full stagger, longer recovery
    Stagger     // Poise broken, significant stun
};

// ============================================================================
// Hit Reaction Configuration
// ============================================================================

struct HitReactionConfig {
    // Animation names for each reaction type
    std::string light_hit_anim = "hit_light";
    std::string medium_hit_anim = "hit_medium";
    std::string heavy_hit_anim = "hit_heavy";
    std::string stagger_anim = "hit_stagger";

    // Damage thresholds (as percentage of max health)
    float light_threshold = 0.05f;   // < 5% max HP = light
    float medium_threshold = 0.15f;  // 5-15% = medium
    float heavy_threshold = 0.25f;   // 15-25% = heavy, >25% = stagger

    // Animation blending
    float blend_in_time = 0.05f;
    float blend_out_time = 0.2f;

    // Animation layer settings
    std::string reaction_layer_name = "hit_reactions";
    int reaction_layer_index = 1;    // Animation layer for reactions

    // Timing
    float cooldown = 0.1f;           // Min time between reactions
    float light_duration = 0.2f;
    float medium_duration = 0.35f;
    float heavy_duration = 0.5f;
    float stagger_duration = 0.8f;

    // Whether this entity can be interrupted by hits
    bool interruptible = true;
};

// ============================================================================
// Hit Reaction Component
// ============================================================================

struct HitReactionComponent {
    HitReactionConfig config;

    // Current state
    bool is_reacting = false;
    float reaction_timer = 0.0f;
    float cooldown_remaining = 0.0f;
    HitReactionType current_reaction = HitReactionType::None;

    // Direction of last hit (for directional reactions)
    core::Vec3 hit_direction{0.0f, 0.0f, 1.0f};

    // Super armor - if > 0, reduces reaction severity
    int super_armor_stacks = 0;
};

// ============================================================================
// Hit Reaction Event
// ============================================================================

struct HitReactionEvent {
    scene::Entity entity;
    HitReactionType type;
    core::Vec3 hit_direction;
    float damage_percent;  // Damage as percent of max health
};

// ============================================================================
// Hit Reaction System
// ============================================================================

class HitReactionSystem {
public:
    static HitReactionSystem& instance();

    // Delete copy/move
    HitReactionSystem(const HitReactionSystem&) = delete;
    HitReactionSystem& operator=(const HitReactionSystem&) = delete;

    // Process damage and trigger appropriate reaction
    // Returns the type of reaction triggered (None if on cooldown or no reaction)
    HitReactionType process_hit(scene::World& world, const DamageInfo& damage);

    // Update active reactions (call each frame)
    void update(scene::World& world, float dt);

    // Query state
    bool is_reacting(scene::World& world, scene::Entity entity) const;
    HitReactionType get_current_reaction(scene::World& world, scene::Entity entity) const;
    float get_reaction_progress(scene::World& world, scene::Entity entity) const;

    // Control
    void cancel_reaction(scene::World& world, scene::Entity entity);
    void force_reaction(scene::World& world, scene::Entity entity, HitReactionType type,
                        const core::Vec3& direction = core::Vec3{0.0f, 0.0f, 1.0f});

    // Super armor management
    void add_super_armor(scene::World& world, scene::Entity entity, int stacks = 1);
    void remove_super_armor(scene::World& world, scene::Entity entity, int stacks = 1);
    void clear_super_armor(scene::World& world, scene::Entity entity);

private:
    HitReactionSystem() = default;
    ~HitReactionSystem() = default;

    HitReactionType determine_type(float damage_percent, const HitReactionConfig& config,
                                    int super_armor_stacks);
    float get_reaction_duration(HitReactionType type, const HitReactionConfig& config) const;
    void start_reaction(scene::World& world, scene::Entity entity, HitReactionType type,
                        HitReactionComponent& comp, const core::Vec3& direction);
    void end_reaction(scene::World& world, scene::Entity entity, HitReactionComponent& comp);
};

// ============================================================================
// ECS System Function
// ============================================================================

// Update hit reactions (register in Update phase)
void hit_reaction_system(scene::World& world, double dt);

// ============================================================================
// Convenience Accessors
// ============================================================================

inline HitReactionSystem& hit_reactions() { return HitReactionSystem::instance(); }

// Helper to get reaction type name
std::string get_reaction_type_name(HitReactionType type);

} // namespace engine::combat

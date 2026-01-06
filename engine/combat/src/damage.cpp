#include <engine/combat/damage.hpp>
#include <engine/combat/hitbox.hpp>
#include <engine/combat/hurtbox.hpp>
#include <engine/combat/iframe.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>
#include <random>

namespace engine::combat {

namespace {

// Random number generator for critical hits
std::mt19937& get_rng() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

float random_float() {
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(get_rng());
}

// Get entity forward direction for backstab detection
Vec3 get_entity_forward(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return -Vec3(world_transform->matrix[2][0],
                     world_transform->matrix[2][1],
                     world_transform->matrix[2][2]);
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->forward();
    }
    return Vec3(0.0f, 0.0f, -1.0f);
}

Vec3 get_entity_position(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return world_transform->get_position();
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position;
    }
    return Vec3(0.0f);
}

} // anonymous namespace

DamageSystem::DamageSystem() {
    // Sort modifiers initially (empty)
}

DamageSystem& DamageSystem::instance() {
    static DamageSystem s_instance;
    return s_instance;
}

DamageInfo DamageSystem::calculate_damage(
    scene::World& world,
    scene::Entity source,
    scene::Entity target,
    const HitboxComponent& hitbox,
    const HurtboxComponent& hurtbox,
    const Vec3& hit_point,
    const Vec3& hit_normal
) {
    DamageInfo info;
    info.source = source;
    info.target = target;
    info.hit_point = hit_point;
    info.hit_normal = hit_normal;
    info.hitbox_id = hitbox.hitbox_id;
    info.hurtbox_type = hurtbox.hurtbox_type;
    info.damage_type = hitbox.damage_type;

    // Start with base damage
    info.raw_damage = hitbox.base_damage;
    info.final_damage = info.raw_damage;

    // Apply hurtbox damage multiplier (weak points, armor)
    info.final_damage *= hurtbox.damage_multiplier;

    // Apply damage type resistance
    float resistance = hurtbox.get_resistance(hitbox.damage_type);
    info.final_damage *= (1.0f - resistance);

    // Check for critical hit
    if (hitbox.critical_chance > 0.0f && random_float() < hitbox.critical_chance) {
        info.is_critical = true;
        info.final_damage *= hitbox.critical_multiplier;
    }

    // Check for backstab
    info.is_backstab = check_backstab(world, source, target,
                                       60.0f); // Default threshold

    auto* receiver = world.try_get<DamageReceiverComponent>(target);
    if (receiver && info.is_backstab && receiver->backstab_vulnerable) {
        info.final_damage *= receiver->backstab_multiplier;
    }

    // Check blocking/parrying
    if (receiver) {
        if (receiver->is_parrying && receiver->parry_window > 0.0f) {
            info.is_parried = true;
            info.final_damage = 0.0f;
        } else if (receiver->is_blocking) {
            info.is_blocked = true;
            info.final_damage *= (1.0f - receiver->block_damage_reduction);
        }
    }

    // Calculate poise damage
    info.poise_damage = hitbox.poise_damage * hurtbox.poise_multiplier;
    if (info.is_blocked) {
        info.poise_damage *= 0.5f; // Reduced poise damage when blocking
    }

    // Calculate knockback
    Vec3 knockback_dir = hitbox.knockback_direction;

    // Transform knockback to world space based on attacker facing
    auto* attacker_transform = world.try_get<scene::WorldTransform>(source);
    if (attacker_transform) {
        Vec4 local_dir(knockback_dir.x, knockback_dir.y, knockback_dir.z, 0.0f);
        Vec4 world_dir = attacker_transform->matrix * local_dir;
        knockback_dir = glm::normalize(Vec3(world_dir.x, world_dir.y, world_dir.z));
    }

    info.knockback = knockback_dir * hitbox.knockback_force;
    if (info.is_blocked) {
        info.knockback *= 0.3f; // Reduced knockback when blocking
    }

    // Apply global modifiers
    apply_modifiers(info);

    // Ensure damage is non-negative
    info.final_damage = std::max(0.0f, info.final_damage);

    return info;
}

DamageInfo DamageSystem::apply_damage(scene::World& world, const DamageInfo& info) {
    DamageInfo result = info;

    // Apply poise damage and check for stagger
    auto* receiver = world.try_get<DamageReceiverComponent>(info.target);
    if (receiver && receiver->can_receive_damage) {
        if (!result.is_parried && result.poise_damage > 0.0f) {
            result.caused_stagger = receiver->apply_poise_damage(result.poise_damage);
        }
    }

    return result;
}

DamageInfo DamageSystem::deal_damage(
    scene::World& world,
    scene::Entity source,
    scene::Entity target,
    const HitboxComponent& hitbox,
    const HurtboxComponent& hurtbox,
    const Vec3& hit_point,
    const Vec3& hit_normal
) {
    DamageInfo info = calculate_damage(world, source, target, hitbox, hurtbox, hit_point, hit_normal);
    return apply_damage(world, info);
}

void DamageSystem::add_modifier(const std::string& name, DamageModifier modifier, int priority) {
    // Remove existing modifier with same name
    remove_modifier(name);

    ModifierEntry entry;
    entry.name = name;
    entry.modifier = std::move(modifier);
    entry.priority = priority;
    m_modifiers.push_back(std::move(entry));

    // Sort by priority (higher first)
    std::sort(m_modifiers.begin(), m_modifiers.end(),
              [](const ModifierEntry& a, const ModifierEntry& b) {
                  return a.priority > b.priority;
              });
}

void DamageSystem::remove_modifier(const std::string& name) {
    m_modifiers.erase(
        std::remove_if(m_modifiers.begin(), m_modifiers.end(),
                       [&name](const ModifierEntry& entry) {
                           return entry.name == name;
                       }),
        m_modifiers.end()
    );
}

void DamageSystem::clear_modifiers() {
    m_modifiers.clear();
}

void DamageSystem::trigger_hitstop(float duration) {
    if (!m_hitstop_enabled) return;

    // Use longer of current or new duration
    m_hitstop_remaining = std::max(m_hitstop_remaining, duration);
}

void DamageSystem::update_hitstop(float dt) {
    if (m_hitstop_remaining > 0.0f) {
        m_hitstop_remaining -= dt;
        if (m_hitstop_remaining < 0.0f) {
            m_hitstop_remaining = 0.0f;
        }
    }
}

float DamageSystem::get_hitstop_time_scale() const {
    return m_hitstop_remaining > 0.0f ? 0.0f : 1.0f;
}

void DamageSystem::apply_modifiers(DamageInfo& info) {
    for (const auto& entry : m_modifiers) {
        if (entry.modifier) {
            entry.modifier(info);
        }
    }
}

bool DamageSystem::check_backstab(scene::World& world, scene::Entity source,
                                   scene::Entity target, float threshold) {
    Vec3 attacker_pos = get_entity_position(world, source);
    Vec3 target_pos = get_entity_position(world, target);
    Vec3 target_forward = get_entity_forward(world, target);

    // Direction from target to attacker
    Vec3 to_attacker = glm::normalize(attacker_pos - target_pos);

    // Dot product: if attacker is behind target, dot will be positive
    // (target facing away from attacker)
    float dot = glm::dot(target_forward, to_attacker);

    // Convert threshold to cosine (60 degrees = cos(120) for behind check)
    float threshold_cos = std::cos(glm::radians(180.0f - threshold));

    return dot > threshold_cos;
}

} // namespace engine::combat

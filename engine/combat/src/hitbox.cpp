#include <engine/combat/hitbox.hpp>
#include <engine/combat/hurtbox.hpp>
#include <engine/combat/damage.hpp>
#include <engine/combat/iframe.hpp>
#include <engine/combat/combat.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <cmath>

namespace engine::combat {

using core::Vec3;
using core::Vec4;

namespace {

// Transform a local offset to world space
Vec3 transform_offset(const Vec3& offset, const scene::WorldTransform& world_transform) {
    Vec4 local_pos(offset.x, offset.y, offset.z, 1.0f);
    Vec4 world_pos = world_transform.matrix * local_pos;
    return Vec3(world_pos.x, world_pos.y, world_pos.z);
}

// Get world position of hitbox/hurtbox center
Vec3 get_shape_world_center(const Vec3& offset, scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (!world_transform) {
        auto* local_transform = world.try_get<scene::LocalTransform>(entity);
        if (local_transform) {
            return local_transform->position + offset;
        }
        return offset;
    }
    return transform_offset(offset, *world_transform);
}

// Get entity forward direction
Vec3 get_entity_forward(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        // Extract forward from rotation matrix (negative Z in our coordinate system)
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

// Check sphere vs sphere overlap
bool sphere_sphere_overlap(const Vec3& a_center, float a_radius,
                           const Vec3& b_center, float b_radius,
                           Vec3& out_hit_point, Vec3& out_hit_normal) {
    Vec3 diff = b_center - a_center;
    float dist_sq = glm::dot(diff, diff);
    float radius_sum = a_radius + b_radius;

    if (dist_sq <= radius_sum * radius_sum) {
        float dist = std::sqrt(dist_sq);
        if (dist > 0.0001f) {
            out_hit_normal = diff / dist;
            out_hit_point = a_center + out_hit_normal * a_radius;
        } else {
            out_hit_normal = Vec3(0.0f, 1.0f, 0.0f);
            out_hit_point = a_center;
        }
        return true;
    }
    return false;
}

// Check box vs box overlap (AABB for simplicity - could extend to OBB)
bool box_box_overlap(const Vec3& a_center, const Vec3& a_half,
                     const Vec3& b_center, const Vec3& b_half,
                     Vec3& out_hit_point, Vec3& out_hit_normal) {
    Vec3 diff = b_center - a_center;
    Vec3 overlap(
        (a_half.x + b_half.x) - std::abs(diff.x),
        (a_half.y + b_half.y) - std::abs(diff.y),
        (a_half.z + b_half.z) - std::abs(diff.z)
    );

    if (overlap.x > 0 && overlap.y > 0 && overlap.z > 0) {
        // Find smallest overlap axis for normal
        if (overlap.x < overlap.y && overlap.x < overlap.z) {
            out_hit_normal = Vec3(diff.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
        } else if (overlap.y < overlap.z) {
            out_hit_normal = Vec3(0.0f, diff.y > 0 ? 1.0f : -1.0f, 0.0f);
        } else {
            out_hit_normal = Vec3(0.0f, 0.0f, diff.z > 0 ? 1.0f : -1.0f);
        }
        out_hit_point = (a_center + b_center) * 0.5f;
        return true;
    }
    return false;
}

// Check capsule vs capsule overlap (simplified - treats as sphere for now)
bool capsule_capsule_overlap(const Vec3& a_center, float a_radius, float a_height,
                              const Vec3& b_center, float b_radius, float b_height,
                              Vec3& out_hit_point, Vec3& out_hit_normal) {
    // Simplified: treat capsule as sphere with extended radius
    float a_effective_radius = a_radius + a_height * 0.5f;
    float b_effective_radius = b_radius + b_height * 0.5f;
    return sphere_sphere_overlap(a_center, a_effective_radius,
                                  b_center, b_effective_radius,
                                  out_hit_point, out_hit_normal);
}

// Generic shape overlap test
bool shapes_overlap(CollisionShape a_shape, const Vec3& a_center, const Vec3& a_half, float a_radius, float a_height,
                    CollisionShape b_shape, const Vec3& b_center, const Vec3& b_half, float b_radius, float b_height,
                    Vec3& out_hit_point, Vec3& out_hit_normal) {
    // Handle all shape combination
    if (a_shape == CollisionShape::Sphere && b_shape == CollisionShape::Sphere) {
        return sphere_sphere_overlap(a_center, a_radius, b_center, b_radius,
                                     out_hit_point, out_hit_normal);
    }
    if (a_shape == CollisionShape::Box && b_shape == CollisionShape::Box) {
        return box_box_overlap(a_center, a_half, b_center, b_half,
                               out_hit_point, out_hit_normal);
    }
    if (a_shape == CollisionShape::Capsule && b_shape == CollisionShape::Capsule) {
        return capsule_capsule_overlap(a_center, a_radius, a_height,
                                        b_center, b_radius, b_height,
                                        out_hit_point, out_hit_normal);
    }

    // Mixed shapes - use conservative sphere approximation
    float a_eff_radius = (a_shape == CollisionShape::Sphere) ? a_radius :
                          (a_shape == CollisionShape::Box) ? glm::length(a_half) :
                          a_radius + a_height * 0.5f;
    float b_eff_radius = (b_shape == CollisionShape::Sphere) ? b_radius :
                          (b_shape == CollisionShape::Box) ? glm::length(b_half) :
                          b_radius + b_height * 0.5f;

    return sphere_sphere_overlap(a_center, a_eff_radius, b_center, b_eff_radius,
                                 out_hit_point, out_hit_normal);
}

// Check if factions are hostile
bool factions_hostile(const HitboxComponent& hitbox, const HurtboxComponent& hurtbox) {
    for (const auto& target_faction : hitbox.target_factions) {
        if (target_faction == hurtbox.faction) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

// Hitbox detection system implementation
void hitbox_detection_system(scene::World& world, double /*dt*/) {
    auto& damage_system = DamageSystem::instance();

    // Get all active hitboxes
    auto hitbox_view = world.view<HitboxComponent, scene::LocalTransform>();

    // Get all enabled hurtboxes
    auto hurtbox_view = world.view<HurtboxComponent, scene::LocalTransform>();

    // Check each hitbox against each hurtbox
    for (auto hitbox_entity : hitbox_view) {
        auto& hitbox = hitbox_view.get<HitboxComponent>(hitbox_entity);

        // Skip inactive hitboxes
        if (!hitbox.active) continue;

        // Get hitbox world position
        Vec3 hitbox_center = get_shape_world_center(hitbox.center_offset, world, hitbox_entity);

        for (auto hurtbox_entity : hurtbox_view) {
            // Skip self
            if (hitbox_entity == hurtbox_entity) continue;

            auto& hurtbox = hurtbox_view.get<HurtboxComponent>(hurtbox_entity);

            // Skip disabled hurtboxes
            if (!hurtbox.enabled) continue;

            // Check faction hostility
            if (!factions_hostile(hitbox, hurtbox)) continue;

            // Skip if already hit this entity
            if (hitbox.was_hit(hurtbox_entity)) continue;

            // Check hit limit
            if (hitbox.max_hits >= 0 &&
                static_cast<int>(hitbox.already_hit.size()) >= hitbox.max_hits) {
                continue;
            }

            // Check i-frames on target
            if (iframe::is_invincible(world, hurtbox_entity)) continue;

            // Get hurtbox world position
            Vec3 hurtbox_center = get_shape_world_center(hurtbox.center_offset, world, hurtbox_entity);

            // Test overlap
            Vec3 hit_point, hit_normal;
            bool overlaps = shapes_overlap(
                hitbox.shape, hitbox_center, hitbox.half_extents, hitbox.radius, hitbox.height,
                hurtbox.shape, hurtbox_center, hurtbox.half_extents, hurtbox.radius, hurtbox.height,
                hit_point, hit_normal
            );

            if (overlaps) {
                // Register hit
                hitbox.already_hit.push_back(hurtbox_entity);

                // Emit hit event
                EntityHitEvent hit_event;
                hit_event.attacker = hitbox_entity;
                hit_event.target = hurtbox_entity;
                hit_event.hit_point = hit_point;
                hit_event.hitbox_id = hitbox.hitbox_id;
                hit_event.hurtbox_type = hurtbox.hurtbox_type;
                core::events().dispatch(hit_event);

                // Calculate and apply damage
                DamageInfo damage_info = damage_system.deal_damage(
                    world, hitbox_entity, hurtbox_entity,
                    hitbox, hurtbox, hit_point, hit_normal
                );

                // Emit damage event
                DamageDealtEvent damage_event;
                damage_event.info = damage_info;
                core::events().dispatch(damage_event);

                // Grant hit i-frames to target
                if (damage_info.final_damage > 0.0f && !damage_info.is_blocked) {
                    iframe::grant_default(world, hurtbox_entity, IFrameSource::Hit);
                }

                // Trigger hitstop
                if (damage_info.final_damage > 0.0f) {
                    damage_system.trigger_hitstop(0.05f);
                }

                // Check for stagger
                if (damage_info.caused_stagger) {
                    EntityStaggeredEvent stagger_event;
                    stagger_event.entity = hurtbox_entity;
                    stagger_event.attacker = hitbox_entity;
                    core::events().dispatch(stagger_event);
                }

                // Check for parry
                if (damage_info.is_parried) {
                    ParryEvent parry_event;
                    parry_event.defender = hurtbox_entity;
                    parry_event.attacker = hitbox_entity;
                    parry_event.hit_point = hit_point;
                    core::events().dispatch(parry_event);
                }

                // Check for block
                if (damage_info.is_blocked) {
                    BlockEvent block_event;
                    block_event.defender = hurtbox_entity;
                    block_event.attacker = hitbox_entity;
                    block_event.blocked_damage = damage_info.raw_damage - damage_info.final_damage;
                    block_event.damage_taken = damage_info.final_damage;
                    core::events().dispatch(block_event);
                }
            }
        }
    }
}

} // namespace engine::combat

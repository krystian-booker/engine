#include <engine/effects/effect_instance.hpp>
#include <engine/effects/effect_definition.hpp>
#include <algorithm>
#include <chrono>

namespace engine::effects {

// ============================================================================
// EffectInstance Implementation
// ============================================================================

bool EffectInstance::should_tick() const {
    const EffectDefinition* def = get_definition();
    return def && def->has_ticking() && is_active();
}

bool EffectInstance::can_refresh() const {
    const EffectDefinition* def = get_definition();
    if (!def) return false;

    return def->stacking == StackBehavior::Refresh ||
           def->stacking == StackBehavior::RefreshExtend ||
           def->stacking == StackBehavior::IntensityRefresh ||
           has_flag(def->flags, EffectFlags::Refreshable);
}

bool EffectInstance::can_add_stack() const {
    const EffectDefinition* def = get_definition();
    if (!def) return false;

    if (def->max_stacks <= 1) return false;
    return stacks < def->max_stacks;
}

float EffectInstance::get_remaining_percent() const {
    if (duration <= 0.0f) return 1.0f;  // Permanent
    return std::clamp(remaining / duration, 0.0f, 1.0f);
}

float EffectInstance::get_elapsed_percent() const {
    if (duration <= 0.0f) return 0.0f;  // Permanent
    return std::clamp(elapsed / duration, 0.0f, 1.0f);
}

void EffectInstance::add_stack(int count) {
    const EffectDefinition* def = get_definition();

    if (def) {
        int old_stacks = stacks;
        stacks = std::min(stacks + count, def->max_stacks);

        // Update intensity based on stacks
        intensity = 1.0f + (stacks - 1) * def->intensity_per_stack;
    } else {
        stacks += count;
    }
}

void EffectInstance::remove_stack(int count) {
    const EffectDefinition* def = get_definition();

    int old_stacks = stacks;
    stacks = std::max(1, stacks - count);

    // Update intensity
    if (def) {
        intensity = 1.0f + (stacks - 1) * def->intensity_per_stack;
    }

    // If stacks reach 0, effect should be removed
    if (stacks <= 0) {
        state = EffectState::Removed;
    }
}

void EffectInstance::set_stacks(int count) {
    const EffectDefinition* def = get_definition();

    if (count <= 0) {
        stacks = 0;
        state = EffectState::Removed;
        return;
    }

    if (def) {
        stacks = std::min(count, def->max_stacks);
        intensity = 1.0f + (stacks - 1) * def->intensity_per_stack;
    } else {
        stacks = count;
    }
}

void EffectInstance::refresh_duration() {
    const EffectDefinition* def = get_definition();

    if (def) {
        duration = def->base_duration * duration_multiplier;
        remaining = duration;
    } else {
        remaining = duration;
    }
}

void EffectInstance::extend_duration(float amount) {
    const EffectDefinition* def = get_definition();

    remaining += amount;

    // Clamp to max duration
    if (def && def->max_duration > 0.0f) {
        float max = def->max_duration * duration_multiplier;
        if (remaining > max) {
            remaining = max;
        }
        duration = std::max(duration, remaining);
    }
}

bool EffectInstance::update(float dt) {
    if (state != EffectState::Active) return false;

    elapsed += dt;

    // Update tick timer
    tick_timer += dt;

    // Update duration if not permanent
    if (duration > 0.0f) {
        remaining -= dt;
        if (remaining <= 0.0f) {
            remaining = 0.0f;
            state = EffectState::Expired;
            return false;
        }
    }

    return true;
}

bool EffectInstance::consume_tick() {
    const EffectDefinition* def = get_definition();
    if (!def || def->tick_interval <= 0.0f) return false;

    if (tick_timer >= def->tick_interval) {
        tick_timer -= def->tick_interval;
        return true;
    }
    return false;
}

const EffectDefinition* EffectInstance::get_definition() const {
    return effect_registry().get(definition_id);
}

EffectInstance EffectInstance::create(const std::string& definition_id,
                                       scene::Entity target,
                                       scene::Entity source) {
    EffectInstance instance;
    instance.instance_id = core::UUID::generate();
    instance.definition_id = definition_id;
    instance.target = target;
    instance.source = source;
    instance.state = EffectState::Pending;

    const EffectDefinition* def = effect_registry().get(definition_id);
    if (def) {
        instance.duration = def->base_duration;
        instance.remaining = def->base_duration;
        instance.stacks = 1;
        instance.intensity = 1.0f;

        // Set tick timer based on tick_on_apply
        if (def->tick_on_apply) {
            instance.tick_timer = def->tick_interval;  // Ready to tick immediately
        } else {
            instance.tick_timer = 0.0f;
        }
    }

    // Set timestamp
    auto now = std::chrono::steady_clock::now();
    instance.apply_timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
    );

    return instance;
}

} // namespace engine::effects

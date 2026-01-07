#include <engine/spline/spline_follower.hpp>
#include <engine/spline/spline_component.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/game_events.hpp>
#include <cmath>
#include <algorithm>

namespace engine::spline {

// SplineFollowerController implementation

void SplineFollowerController::play(SplineFollowerComponent& follower) {
    follower.is_moving = true;
}

void SplineFollowerController::pause(SplineFollowerComponent& follower) {
    follower.is_moving = false;
}

void SplineFollowerController::stop(SplineFollowerComponent& follower) {
    follower.is_moving = false;
    follower.current_distance = 0.0f;
    follower.current_t = 0.0f;
    follower.is_reversed = false;
    follower.current_loop = 0;
    follower.has_started = false;
}

void SplineFollowerController::toggle(SplineFollowerComponent& follower) {
    follower.is_moving = !follower.is_moving;
}

void SplineFollowerController::set_position(SplineFollowerComponent& follower, float t) {
    follower.current_t = std::clamp(t, 0.0f, 1.0f);
    // Distance will be recalculated on next update
}

void SplineFollowerController::set_distance(SplineFollowerComponent& follower, float distance) {
    follower.current_distance = std::max(0.0f, distance);
    // t will be recalculated on next update
}

void SplineFollowerController::jump_to_start(SplineFollowerComponent& follower) {
    follower.current_distance = 0.0f;
    follower.current_t = 0.0f;
}

void SplineFollowerController::jump_to_end(SplineFollowerComponent& follower) {
    follower.current_t = 1.0f;
    // Distance will be set to spline length on next update
}

void SplineFollowerController::reverse(SplineFollowerComponent& follower) {
    follower.is_reversed = !follower.is_reversed;
}

void SplineFollowerController::set_reversed(SplineFollowerComponent& follower, bool reversed) {
    follower.is_reversed = reversed;
}

void SplineFollowerController::set_speed(SplineFollowerComponent& follower, float speed) {
    follower.speed = speed;
}

void SplineFollowerController::multiply_speed(SplineFollowerComponent& follower, float multiplier) {
    follower.speed *= multiplier;
}

// Helper functions

static float apply_easing(float t, SplineFollowerComponent::EaseType type) {
    switch (type) {
        case SplineFollowerComponent::EaseType::None:
            return t;
        case SplineFollowerComponent::EaseType::EaseIn:
            return t * t;
        case SplineFollowerComponent::EaseType::EaseOut:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case SplineFollowerComponent::EaseType::EaseInOut:
            return (t < 0.5f) ? (2.0f * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f);
        case SplineFollowerComponent::EaseType::Custom:
            return t; // Custom would use a curve
    }
    return t;
}

static Quat compute_orientation(const SplineEvalResult& eval, const SplineFollowerComponent& follower,
                                 engine::scene::World& world) {
    switch (follower.orientation) {
        case FollowOrientation::None:
            return Quat(1.0f, 0.0f, 0.0f, 0.0f);

        case FollowOrientation::FollowTangent: {
            // Create rotation from tangent
            Vec3 forward = follower.is_reversed ? -eval.tangent : eval.tangent;
            Vec3 up = follower.up_vector;

            // Handle case where forward is parallel to up
            if (std::abs(glm::dot(forward, up)) > 0.99f) {
                up = Vec3(1.0f, 0.0f, 0.0f);
                if (std::abs(glm::dot(forward, up)) > 0.99f) {
                    up = Vec3(0.0f, 0.0f, 1.0f);
                }
            }

            Vec3 right = glm::normalize(glm::cross(up, forward));
            up = glm::cross(forward, right);

            Mat3 rot_mat(right, up, forward);
            return glm::quat_cast(rot_mat);
        }

        case FollowOrientation::FollowPath: {
            Vec3 forward = follower.is_reversed ? -eval.tangent : eval.tangent;
            Vec3 up = eval.normal;
            Vec3 right = eval.binormal;

            Mat3 rot_mat(right, up, forward);
            Quat result = glm::quat_cast(rot_mat);

            // Apply roll from spline
            if (std::abs(eval.roll) > 0.0001f) {
                result = result * glm::angleAxis(eval.roll, Vec3(0.0f, 0.0f, 1.0f));
            }

            return result;
        }

        case FollowOrientation::LookAt: {
            Vec3 target_pos{0.0f};
            if (follower.look_at_entity != scene::NullEntity) {
                auto* target_transform = world.try_get<scene::WorldTransform>(follower.look_at_entity);
                if (target_transform) {
                    target_pos = target_transform->position() + follower.look_at_offset;
                }
            } else {
                target_pos = follower.look_at_offset;
            }

            Vec3 forward = glm::normalize(target_pos - eval.position);
            Vec3 up = follower.up_vector;

            if (std::abs(glm::dot(forward, up)) > 0.99f) {
                up = Vec3(1.0f, 0.0f, 0.0f);
            }

            Vec3 right = glm::normalize(glm::cross(up, forward));
            up = glm::cross(forward, right);

            Mat3 rot_mat(right, up, forward);
            return glm::quat_cast(rot_mat);
        }

        case FollowOrientation::Custom:
            return Quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    return Quat(1.0f, 0.0f, 0.0f, 0.0f);
}

// Systems

void spline_follower_system(engine::scene::World& world, double dt) {
    float fdt = static_cast<float>(dt);

    auto view = world.view<SplineFollowerComponent, scene::LocalTransform>();
    for (auto entity : view) {
        auto& follower = view.get<SplineFollowerComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        // Check if spline entity is valid
        if (follower.spline_entity == scene::NullEntity) continue;
        auto* spline_comp = world.try_get<SplineComponent>(follower.spline_entity);
        if (!spline_comp) continue;

        const Spline* spline = spline_comp->get_spline();
        if (!spline || spline->point_count() < 2) continue;

        float spline_length = spline->get_length();
        if (spline_length < 0.0001f) continue;

        // Fire started event
        if (!follower.has_started && follower.fire_started_event) {
            follower.has_started = true;
            // core::game_events().broadcast({follower.started_event_name, entity});
        }

        // Update position based on mode
        if (follower.is_moving) {
            float delta = 0.0f;

            switch (follower.follow_mode) {
                case FollowMode::Distance:
                    delta = follower.speed * fdt;
                    break;

                case FollowMode::Parameter:
                    delta = follower.parameter_speed * fdt * spline_length;
                    break;

                case FollowMode::Time:
                    delta = (spline_length / follower.duration) * fdt;
                    break;
            }

            // Apply direction
            if (follower.is_reversed) {
                delta = -delta;
            }

            // Update distance
            float new_distance = follower.current_distance + delta;

            // Handle end conditions
            bool reached_end = false;
            if (new_distance >= spline_length) {
                reached_end = true;
                switch (follower.end_behavior) {
                    case FollowEndBehavior::Stop:
                        new_distance = spline_length;
                        follower.is_moving = false;
                        break;

                    case FollowEndBehavior::Loop:
                        new_distance = std::fmod(new_distance, spline_length);
                        follower.current_loop++;
                        if (follower.fire_loop_event) {
                            // core::game_events().broadcast({follower.loop_event_name, entity});
                        }
                        if (follower.max_loops >= 0 && follower.current_loop >= follower.max_loops) {
                            new_distance = spline_length;
                            follower.is_moving = false;
                        }
                        break;

                    case FollowEndBehavior::PingPong:
                        follower.is_reversed = !follower.is_reversed;
                        new_distance = spline_length - (new_distance - spline_length);
                        follower.current_loop++;
                        if (follower.fire_loop_event) {
                            // core::game_events().broadcast({follower.loop_event_name, entity});
                        }
                        break;

                    case FollowEndBehavior::Destroy:
                        world.destroy(entity);
                        continue;

                    case FollowEndBehavior::Custom:
                        new_distance = spline_length;
                        follower.is_moving = false;
                        break;
                }
            } else if (new_distance < 0.0f) {
                reached_end = true;
                switch (follower.end_behavior) {
                    case FollowEndBehavior::Stop:
                        new_distance = 0.0f;
                        follower.is_moving = false;
                        break;

                    case FollowEndBehavior::Loop:
                        new_distance = spline_length + new_distance;
                        follower.current_loop++;
                        break;

                    case FollowEndBehavior::PingPong:
                        follower.is_reversed = !follower.is_reversed;
                        new_distance = -new_distance;
                        follower.current_loop++;
                        break;

                    case FollowEndBehavior::Destroy:
                        world.destroy(entity);
                        continue;

                    case FollowEndBehavior::Custom:
                        new_distance = 0.0f;
                        follower.is_moving = false;
                        break;
                }
            }

            follower.current_distance = new_distance;
            follower.current_t = spline->get_t_at_distance(new_distance);

            if (reached_end && !follower.is_moving && follower.fire_ended_event) {
                // core::game_events().broadcast({follower.ended_event_name, entity});
            }
        }

        // Apply easing
        float display_t = follower.current_t;
        if (follower.ease_in != SplineFollowerComponent::EaseType::None && follower.current_distance < follower.ease_distance) {
            float ease_t = follower.current_distance / follower.ease_distance;
            float eased = apply_easing(ease_t, follower.ease_in);
            display_t = eased * (follower.ease_distance / spline_length);
        }
        if (follower.ease_out != SplineFollowerComponent::EaseType::None &&
            follower.current_distance > spline_length - follower.ease_distance) {
            float remaining = spline_length - follower.current_distance;
            float ease_t = remaining / follower.ease_distance;
            float eased = apply_easing(ease_t, follower.ease_out);
            display_t = 1.0f - eased * (follower.ease_distance / spline_length);
        }

        // Evaluate spline at current position
        SplineEvalResult eval = spline->evaluate(display_t);

        // Apply offset
        Vec3 final_position = eval.position;
        if (glm::length(follower.offset) > 0.0001f) {
            if (follower.offset_in_spline_space) {
                final_position += eval.binormal * follower.offset.x;
                final_position += eval.normal * follower.offset.y;
                final_position += eval.tangent * follower.offset.z;
            } else {
                final_position += follower.offset;
            }
        }

        // Update transform
        transform.position = final_position;

        // Update rotation
        if (follower.orientation != FollowOrientation::None) {
            Quat target_rot = compute_orientation(eval, follower, world);

            if (follower.rotation_smoothing > 0.0f) {
                float smooth_factor = 1.0f - std::exp(-follower.rotation_smoothing * fdt);
                transform.rotation = glm::slerp(follower.current_rotation, target_rot, smooth_factor);
                follower.current_rotation = transform.rotation;
            } else {
                transform.rotation = target_rot;
            }

            follower.target_rotation = target_rot;
        }
    }
}

void spline_attachment_system(engine::scene::World& world, double /*dt*/) {
    auto view = world.view<SplineAttachmentComponent, scene::LocalTransform>();
    for (auto entity : view) {
        auto& attachment = view.get<SplineAttachmentComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        if (attachment.spline_entity == scene::NullEntity) continue;
        auto* spline_comp = world.try_get<SplineComponent>(attachment.spline_entity);
        if (!spline_comp) continue;

        const Spline* spline = spline_comp->get_spline();
        if (!spline || spline->point_count() < 2) continue;

        // Evaluate at position
        SplineEvalResult eval;
        if (attachment.use_distance) {
            eval = spline->evaluate_at_distance(attachment.distance);
        } else {
            eval = spline->evaluate(attachment.t);
        }

        // Apply offset
        Vec3 final_position = eval.position;
        if (glm::length(attachment.offset) > 0.0001f) {
            if (attachment.offset_in_spline_space) {
                final_position += eval.binormal * attachment.offset.x;
                final_position += eval.normal * attachment.offset.y;
                final_position += eval.tangent * attachment.offset.z;
            } else {
                final_position += attachment.offset;
            }
        }

        transform.position = final_position;

        // Match rotation
        if (attachment.match_rotation) {
            Vec3 forward = eval.tangent;
            Vec3 up = eval.normal;
            Vec3 right = eval.binormal;

            Mat3 rot_mat(right, up, forward);
            transform.rotation = glm::quat_cast(rot_mat);

            // Apply additional rotation offset
            if (glm::length(attachment.rotation_offset) > 0.0001f) {
                Quat offset_rot = glm::quat(attachment.rotation_offset);
                transform.rotation = transform.rotation * offset_rot;
            }
        }
    }
}

void setup_spline_follower(engine::scene::World& world, scene::Entity follower_entity,
                           scene::Entity spline_entity, float speed,
                           FollowEndBehavior end_behavior) {
    auto& follower = world.emplace<SplineFollowerComponent>(follower_entity);
    follower.spline_entity = spline_entity;
    follower.speed = speed;
    follower.end_behavior = end_behavior;
    follower.is_moving = true;
}

void attach_to_spline(engine::scene::World& world, scene::Entity entity,
                      scene::Entity spline_entity, float t) {
    auto& attachment = world.emplace<SplineAttachmentComponent>(entity);
    attachment.spline_entity = spline_entity;
    attachment.t = t;
    attachment.use_distance = false;
}

} // namespace engine::spline

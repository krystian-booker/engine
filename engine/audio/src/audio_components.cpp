#include <engine/audio/audio_components.hpp>
#include <cmath>
#include <algorithm>

namespace engine::audio {

float calculate_attenuation(float distance, float min_dist, float max_dist,
                           AttenuationModel model, float rolloff) {
    // Guard against invalid distance ranges
    if (max_dist <= min_dist) {
        return 1.0f;
    }

    // Ensure min_dist is positive to avoid division by zero in ratio calculations
    min_dist = std::max(min_dist, 0.001f);

    if (distance <= min_dist) {
        return 1.0f;
    }

    if (distance >= max_dist) {
        return 0.0f;
    }

    float normalized_dist = (distance - min_dist) / (max_dist - min_dist);

    switch (model) {
        case AttenuationModel::None:
            return 1.0f;

        case AttenuationModel::Linear:
            return 1.0f - normalized_dist;

        case AttenuationModel::InverseSquare: {
            // 1 / (1 + rolloff * (d/min_dist - 1)^2)
            float ratio = distance / min_dist;
            return 1.0f / (1.0f + rolloff * (ratio - 1.0f) * (ratio - 1.0f));
        }

        case AttenuationModel::Logarithmic: {
            // Based on decibel scaling
            float ratio = distance / min_dist;
            float db = -20.0f * rolloff * std::log10(ratio);
            return std::pow(10.0f, db / 20.0f);
        }

        default:
            return 1.0f;
    }
}

float calculate_cone_attenuation(const Vec3& source_forward,
                                  const Vec3& to_listener,
                                  float inner_angle,
                                  float outer_angle,
                                  float outer_volume) {
    // Calculate angle between source direction and listener direction
    float cos_angle = glm::dot(glm::normalize(source_forward), glm::normalize(-to_listener));
    float angle = std::acos(glm::clamp(cos_angle, -1.0f, 1.0f));
    float angle_degrees = glm::degrees(angle);

    float half_inner = inner_angle * 0.5f;
    float half_outer = outer_angle * 0.5f;

    // Guard against degenerate cone (inner >= outer)
    if (half_outer <= half_inner) {
        return 1.0f;
    }

    if (angle_degrees <= half_inner) {
        // Inside inner cone - full volume
        return 1.0f;
    } else if (angle_degrees >= half_outer) {
        // Outside outer cone
        return outer_volume;
    } else {
        // Between cones - interpolate
        float t = (angle_degrees - half_inner) / (half_outer - half_inner);
        return glm::mix(1.0f, outer_volume, t);
    }
}

} // namespace engine::audio

#pragma once

#include <engine/ai/behavior_tree.hpp>
#include <engine/ai/blackboard.hpp>
#include <random>

namespace engine::ai {

// ============================================================================
// Wait Node
// Waits for a specified duration
// ============================================================================

class BTWait : public BTLeafNode {
public:
    explicit BTWait(float duration = 1.0f, std::string name = "Wait")
        : BTLeafNode(std::move(name))
        , m_duration(duration) {}

    BTStatus tick(BTContext& ctx) override {
        m_elapsed += ctx.delta_time;

        if (m_elapsed >= m_duration) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        m_last_status = BTStatus::Running;
        return BTStatus::Running;
    }

    void reset() override {
        m_elapsed = 0.0f;
    }

    void set_duration(float duration) { m_duration = duration; }

private:
    float m_duration;
    float m_elapsed = 0.0f;
};

// ============================================================================
// Wait Random
// Waits for a random duration within a range
// ============================================================================

class BTWaitRandom : public BTLeafNode {
public:
    BTWaitRandom(float min_duration = 0.5f, float max_duration = 2.0f,
                 std::string name = "WaitRandom")
        : BTLeafNode(std::move(name))
        , m_min_duration(min_duration)
        , m_max_duration(max_duration) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_started) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dist(m_min_duration, m_max_duration);
            m_target_duration = dist(gen);
            m_started = true;
            m_elapsed = 0.0f;
        }

        m_elapsed += ctx.delta_time;

        if (m_elapsed >= m_target_duration) {
            m_started = false;
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        m_last_status = BTStatus::Running;
        return BTStatus::Running;
    }

    void reset() override {
        m_started = false;
        m_elapsed = 0.0f;
    }

private:
    float m_min_duration;
    float m_max_duration;
    float m_target_duration = 0.0f;
    float m_elapsed = 0.0f;
    bool m_started = false;
};

// ============================================================================
// Set Blackboard Value
// ============================================================================

template<typename T>
class BTSetBlackboard : public BTLeafNode {
public:
    BTSetBlackboard(std::string key, T value, std::string name = "SetBlackboard")
        : BTLeafNode(std::move(name))
        , m_key(std::move(key))
        , m_value(std::move(value)) {}

    BTStatus tick(BTContext& ctx) override {
        if (ctx.blackboard) {
            ctx.blackboard->set<T>(m_key, m_value);
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }
        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }

private:
    std::string m_key;
    T m_value;
};

// ============================================================================
// Check Blackboard Value
// ============================================================================

class BTCheckBlackboardBool : public BTLeafNode {
public:
    BTCheckBlackboardBool(std::string key, bool expected_value = true,
                          std::string name = "CheckBlackboard")
        : BTLeafNode(std::move(name))
        , m_key(std::move(key))
        , m_expected(expected_value) {}

    BTStatus tick(BTContext& ctx) override {
        if (!ctx.blackboard) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        bool value = ctx.blackboard->get_bool(m_key, !m_expected);
        m_last_status = (value == m_expected) ? BTStatus::Success : BTStatus::Failure;
        return m_last_status;
    }

private:
    std::string m_key;
    bool m_expected;
};

class BTCheckBlackboardFloat : public BTLeafNode {
public:
    enum class Comparison { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

    BTCheckBlackboardFloat(std::string key, Comparison comp, float value,
                           std::string name = "CheckBlackboardFloat")
        : BTLeafNode(std::move(name))
        , m_key(std::move(key))
        , m_comparison(comp)
        , m_value(value) {}

    BTStatus tick(BTContext& ctx) override {
        if (!ctx.blackboard) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        float bb_value = ctx.blackboard->get_float(m_key, 0.0f);
        bool result = false;

        switch (m_comparison) {
            case Comparison::Equal: result = (bb_value == m_value); break;
            case Comparison::NotEqual: result = (bb_value != m_value); break;
            case Comparison::Less: result = (bb_value < m_value); break;
            case Comparison::LessEqual: result = (bb_value <= m_value); break;
            case Comparison::Greater: result = (bb_value > m_value); break;
            case Comparison::GreaterEqual: result = (bb_value >= m_value); break;
        }

        m_last_status = result ? BTStatus::Success : BTStatus::Failure;
        return m_last_status;
    }

private:
    std::string m_key;
    Comparison m_comparison;
    float m_value;
};

// ============================================================================
// Check Has Target
// ============================================================================

class BTHasTarget : public BTLeafNode {
public:
    explicit BTHasTarget(std::string target_key = bb::TARGET_ENTITY,
                         std::string name = "HasTarget")
        : BTLeafNode(std::move(name))
        , m_target_key(std::move(target_key)) {}

    BTStatus tick(BTContext& ctx) override {
        if (!ctx.blackboard) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        scene::Entity target = ctx.blackboard->get_entity(m_target_key);
        m_last_status = (target != scene::NullEntity) ? BTStatus::Success : BTStatus::Failure;
        return m_last_status;
    }

private:
    std::string m_target_key;
};

// ============================================================================
// Clear Target
// ============================================================================

class BTClearTarget : public BTLeafNode {
public:
    explicit BTClearTarget(std::string target_key = bb::TARGET_ENTITY,
                           std::string name = "ClearTarget")
        : BTLeafNode(std::move(name))
        , m_target_key(std::move(target_key)) {}

    BTStatus tick(BTContext& ctx) override {
        if (ctx.blackboard) {
            ctx.blackboard->set_entity(m_target_key, scene::NullEntity);
        }
        m_last_status = BTStatus::Success;
        return BTStatus::Success;
    }

private:
    std::string m_target_key;
};

// ============================================================================
// Log Node (for debugging)
// ============================================================================

class BTLog : public BTLeafNode {
public:
    BTLog(std::string message, BTStatus return_status = BTStatus::Success,
          std::string name = "Log")
        : BTLeafNode(std::move(name))
        , m_message(std::move(message))
        , m_return_status(return_status) {}

    BTStatus tick(BTContext& ctx) override {
        // In production, this would use the engine's logging system
        // For now, just set the status
        m_last_status = m_return_status;
        return m_return_status;
    }

private:
    std::string m_message;
    BTStatus m_return_status;
};

// ============================================================================
// Is In Range
// Checks if target is within specified distance
// ============================================================================

class BTIsInRange : public BTLeafNode {
public:
    BTIsInRange(float range, std::string target_key = bb::TARGET_POSITION,
                std::string name = "IsInRange")
        : BTLeafNode(std::move(name))
        , m_range(range)
        , m_target_key(std::move(target_key)) {}

    BTStatus tick(BTContext& ctx) override;

    void set_range(float range) { m_range = range; }

private:
    float m_range;
    std::string m_target_key;
};

// ============================================================================
// Move To Target
// Uses navigation to move toward a target position
// ============================================================================

class BTMoveTo : public BTLeafNode {
public:
    BTMoveTo(std::string target_key = bb::MOVE_TARGET,
             float arrival_distance = 0.5f,
             std::string name = "MoveTo")
        : BTLeafNode(std::move(name))
        , m_target_key(std::move(target_key))
        , m_arrival_distance(arrival_distance) {}

    BTStatus tick(BTContext& ctx) override;

    void reset() override {
        m_path_requested = false;
    }

    void set_arrival_distance(float distance) { m_arrival_distance = distance; }
    void set_movement_speed(float speed) { m_movement_speed = speed; }

private:
    std::string m_target_key;
    float m_arrival_distance;
    float m_movement_speed = 5.0f;
    bool m_path_requested = false;
};

// ============================================================================
// Look At Target
// Rotates entity to face target
// ============================================================================

class BTLookAt : public BTLeafNode {
public:
    BTLookAt(std::string target_key = bb::TARGET_POSITION,
             float rotation_speed = 360.0f,
             std::string name = "LookAt")
        : BTLeafNode(std::move(name))
        , m_target_key(std::move(target_key))
        , m_rotation_speed(rotation_speed) {}

    BTStatus tick(BTContext& ctx) override;

    void set_rotation_speed(float speed) { m_rotation_speed = speed; }

private:
    std::string m_target_key;
    float m_rotation_speed;
};

// ============================================================================
// Play Animation
// Triggers an animation on the entity
// ============================================================================

class BTPlayAnimation : public BTLeafNode {
public:
    BTPlayAnimation(std::string animation_name, bool wait_for_completion = true,
                    std::string name = "PlayAnimation")
        : BTLeafNode(std::move(name))
        , m_animation_name(std::move(animation_name))
        , m_wait_for_completion(wait_for_completion) {}

    BTStatus tick(BTContext& ctx) override;

    void reset() override {
        m_animation_started = false;
    }

private:
    std::string m_animation_name;
    bool m_wait_for_completion;
    bool m_animation_started = false;
};

// ============================================================================
// Play Sound
// Plays a sound effect
// ============================================================================

class BTPlaySound : public BTLeafNode {
public:
    explicit BTPlaySound(std::string sound_name, std::string name = "PlaySound")
        : BTLeafNode(std::move(name))
        , m_sound_name(std::move(sound_name)) {}

    BTStatus tick(BTContext& ctx) override;

private:
    std::string m_sound_name;
};

// ============================================================================
// Random Chance
// Succeeds with specified probability
// ============================================================================

class BTRandomChance : public BTLeafNode {
public:
    explicit BTRandomChance(float probability = 0.5f, std::string name = "RandomChance")
        : BTLeafNode(std::move(name))
        , m_probability(probability) {}

    BTStatus tick(BTContext& ctx) override {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        m_last_status = (dist(gen) < m_probability) ? BTStatus::Success : BTStatus::Failure;
        return m_last_status;
    }

    void set_probability(float prob) { m_probability = prob; }

private:
    float m_probability;
};

} // namespace engine::ai

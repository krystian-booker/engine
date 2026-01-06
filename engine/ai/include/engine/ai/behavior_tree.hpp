#pragma once

#include <engine/ai/blackboard.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace engine::ai {

// ============================================================================
// Behavior Tree Status
// ============================================================================

enum class BTStatus : uint8_t {
    Success,    // Node completed successfully
    Failure,    // Node failed
    Running     // Node still executing
};

// Convert status to string for debugging
inline const char* to_string(BTStatus status) {
    switch (status) {
        case BTStatus::Success: return "Success";
        case BTStatus::Failure: return "Failure";
        case BTStatus::Running: return "Running";
        default: return "Unknown";
    }
}

// ============================================================================
// Behavior Tree Context
// ============================================================================

struct BTContext {
    scene::World* world = nullptr;
    scene::Entity entity = scene::NullEntity;
    Blackboard* blackboard = nullptr;
    float delta_time = 0.0f;

    // Helpers
    bool is_valid() const {
        return world != nullptr && entity != scene::NullEntity && blackboard != nullptr;
    }
};

// ============================================================================
// Behavior Tree Node Interface
// ============================================================================

class IBTNode {
public:
    virtual ~IBTNode() = default;

    // Execute the node
    virtual BTStatus tick(BTContext& ctx) = 0;

    // Reset the node state (called when tree is reset or node needs re-initialization)
    virtual void reset() {}

    // Get node name for debugging
    virtual const std::string& get_name() const { return m_name; }

    // For debugging/visualization
    BTStatus get_last_status() const { return m_last_status; }

protected:
    std::string m_name = "BTNode";
    BTStatus m_last_status = BTStatus::Failure;

    void set_status(BTStatus status) { m_last_status = status; }
};

using BTNodePtr = std::unique_ptr<IBTNode>;

// ============================================================================
// Leaf Node (Action/Condition base)
// ============================================================================

class BTLeafNode : public IBTNode {
public:
    explicit BTLeafNode(std::string name = "LeafNode") {
        m_name = std::move(name);
    }
};

// ============================================================================
// Action Node (performs an action)
// ============================================================================

// Lambda-based action node
class BTAction : public BTLeafNode {
public:
    using ActionFn = std::function<BTStatus(BTContext&)>;

    BTAction(std::string name, ActionFn action)
        : BTLeafNode(std::move(name))
        , m_action(std::move(action)) {}

    BTStatus tick(BTContext& ctx) override {
        if (m_action) {
            m_last_status = m_action(ctx);
        } else {
            m_last_status = BTStatus::Failure;
        }
        return m_last_status;
    }

private:
    ActionFn m_action;
};

// ============================================================================
// Condition Node (checks a condition)
// ============================================================================

// Lambda-based condition node
class BTCondition : public BTLeafNode {
public:
    using ConditionFn = std::function<bool(const BTContext&)>;

    BTCondition(std::string name, ConditionFn condition)
        : BTLeafNode(std::move(name))
        , m_condition(std::move(condition)) {}

    BTStatus tick(BTContext& ctx) override {
        if (m_condition && m_condition(ctx)) {
            m_last_status = BTStatus::Success;
        } else {
            m_last_status = BTStatus::Failure;
        }
        return m_last_status;
    }

private:
    ConditionFn m_condition;
};

// ============================================================================
// Composite Node (has children)
// ============================================================================

class BTComposite : public IBTNode {
public:
    explicit BTComposite(std::string name = "Composite") {
        m_name = std::move(name);
    }

    void add_child(BTNodePtr child) {
        if (child) {
            m_children.push_back(std::move(child));
        }
    }

    template<typename T, typename... Args>
    T* add_child(Args&&... args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = child.get();
        m_children.push_back(std::move(child));
        return ptr;
    }

    size_t get_child_count() const { return m_children.size(); }

    void reset() override {
        m_current_child = 0;
        for (auto& child : m_children) {
            child->reset();
        }
    }

protected:
    std::vector<BTNodePtr> m_children;
    size_t m_current_child = 0;
};

// ============================================================================
// Decorator Node (wraps a single child)
// ============================================================================

class BTDecorator : public IBTNode {
public:
    explicit BTDecorator(std::string name = "Decorator") {
        m_name = std::move(name);
    }

    void set_child(BTNodePtr child) {
        m_child = std::move(child);
    }

    template<typename T, typename... Args>
    T* set_child(Args&&... args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = child.get();
        m_child = std::move(child);
        return ptr;
    }

    bool has_child() const { return m_child != nullptr; }

    void reset() override {
        if (m_child) {
            m_child->reset();
        }
    }

protected:
    BTNodePtr m_child;
};

// ============================================================================
// Behavior Tree
// ============================================================================

class BehaviorTree {
public:
    BehaviorTree() = default;
    explicit BehaviorTree(std::string name) : m_name(std::move(name)) {}

    // Set root node
    void set_root(BTNodePtr root) {
        m_root = std::move(root);
    }

    template<typename T, typename... Args>
    T* set_root(Args&&... args) {
        auto root = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = root.get();
        m_root = std::move(root);
        return ptr;
    }

    // Execute the tree
    BTStatus tick(BTContext& ctx) {
        if (!m_root) {
            return BTStatus::Failure;
        }
        m_last_status = m_root->tick(ctx);
        return m_last_status;
    }

    // Reset the tree
    void reset() {
        if (m_root) {
            m_root->reset();
        }
    }

    // Getters
    const std::string& get_name() const { return m_name; }
    BTStatus get_last_status() const { return m_last_status; }
    IBTNode* get_root() const { return m_root.get(); }

private:
    std::string m_name = "BehaviorTree";
    BTNodePtr m_root;
    BTStatus m_last_status = BTStatus::Failure;
};

using BehaviorTreePtr = std::shared_ptr<BehaviorTree>;

// ============================================================================
// Builder Helpers
// ============================================================================

// Create an action node from a lambda
inline BTNodePtr make_action(std::string name, BTAction::ActionFn action) {
    return std::make_unique<BTAction>(std::move(name), std::move(action));
}

// Create a condition node from a lambda
inline BTNodePtr make_condition(std::string name, BTCondition::ConditionFn condition) {
    return std::make_unique<BTCondition>(std::move(name), std::move(condition));
}

} // namespace engine::ai

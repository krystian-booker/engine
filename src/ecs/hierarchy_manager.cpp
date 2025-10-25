#include "hierarchy_manager.h"
#include <algorithm>

void HierarchyManager::SetParent(Entity child, Entity parent) {
    // Remove from old parent if exists
    auto it = m_Parents.find(child);
    if (it != m_Parents.end()) {
        RemoveChildFromParent(child, it->second);
    }

    // Set new parent
    m_Parents[child] = parent;
    m_Children[parent].push_back(child);
}

void HierarchyManager::RemoveParent(Entity child) {
    auto it = m_Parents.find(child);
    if (it != m_Parents.end()) {
        RemoveChildFromParent(child, it->second);
        m_Parents.erase(it);
    }
}

Entity HierarchyManager::GetParent(Entity child) const {
    auto it = m_Parents.find(child);
    return (it != m_Parents.end()) ? it->second : Entity::Invalid;
}

const std::vector<Entity>& HierarchyManager::GetChildren(Entity parent) const {
    static std::vector<Entity> empty;
    auto it = m_Children.find(parent);
    return (it != m_Children.end()) ? it->second : empty;
}

bool HierarchyManager::HasChildren(Entity entity) const {
    auto it = m_Children.find(entity);
    return it != m_Children.end() && !it->second.empty();
}

void HierarchyManager::OnEntityDestroyed(Entity entity) {
    // Remove from parent's children
    RemoveParent(entity);

    // Orphan all children (make them roots)
    auto childIt = m_Children.find(entity);
    if (childIt != m_Children.end()) {
        for (Entity child : childIt->second) {
            m_Parents.erase(child);
        }
        m_Children.erase(childIt);
    }
}

std::vector<Entity> HierarchyManager::GetRootEntities() const {
    std::vector<Entity> roots;

    // All entities in children map that are not in parents map are roots
    for (const auto& [entity, children] : m_Children) {
        if (m_Parents.find(entity) == m_Parents.end()) {
            roots.push_back(entity);
        }
    }

    // Also entities in parents map with no parent
    for (const auto& [child, parent] : m_Parents) {
        if (!parent.IsValid() &&
            std::find(roots.begin(), roots.end(), child) == roots.end()) {
            roots.push_back(child);
        }
    }

    return roots;
}

void HierarchyManager::TraverseDepthFirst(Entity root, std::function<void(Entity)> callback) const {
    callback(root);

    const auto& children = GetChildren(root);
    for (Entity child : children) {
        TraverseDepthFirst(child, callback);
    }
}

void HierarchyManager::RemoveChildFromParent(Entity child, Entity parent) {
    auto it = m_Children.find(parent);
    if (it != m_Children.end()) {
        auto& children = it->second;
        children.erase(std::remove(children.begin(), children.end(), child), children.end());

        if (children.empty()) {
            m_Children.erase(it);
        }
    }
}

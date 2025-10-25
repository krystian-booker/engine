#pragma once
#include "entity.h"
#include <vector>
#include <unordered_map>
#include <functional>

// HierarchyManager: Manages parent-child relationships separately from Transform component
// Goal: Keep Transform component as pure data (SoA friendly) while hierarchy is relational
class HierarchyManager {
public:
    HierarchyManager() = default;
    ~HierarchyManager() = default;

    // Set parent-child relationship
    void SetParent(Entity child, Entity parent);

    // Remove parent (make root)
    void RemoveParent(Entity child);

    // Get parent (returns Invalid if root)
    Entity GetParent(Entity child) const;

    // Get children
    const std::vector<Entity>& GetChildren(Entity parent) const;

    // Check if entity has children
    bool HasChildren(Entity entity) const;

    // Remove entity from hierarchy (call on destroy)
    void OnEntityDestroyed(Entity entity);

    // Get all root entities (no parent)
    std::vector<Entity> GetRootEntities() const;

    // Depth-first traversal from root
    void TraverseDepthFirst(Entity root, std::function<void(Entity)> callback) const;

private:
    // parent -> list of children
    std::unordered_map<Entity, std::vector<Entity>> m_Children;

    // child -> parent
    std::unordered_map<Entity, Entity> m_Parents;

    // Helper: Remove child from parent's list
    void RemoveChildFromParent(Entity child, Entity parent);
};

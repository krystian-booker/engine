#pragma once
#include "ecs/component_registry.h"
#include "ecs/hierarchy_manager.h"
#include "ecs/components/transform.h"

class TransformSystem {
public:
    TransformSystem(ComponentRegistry* registry, HierarchyManager* hierarchy)
        : m_Registry(registry), m_Hierarchy(hierarchy) {}

    void Update(float deltaTime);

private:
    ComponentRegistry* m_Registry;
    HierarchyManager* m_Hierarchy;

    // Update single entity and propagate to children
    void UpdateTransformRecursive(Entity entity, const Mat4& parentWorld);
};

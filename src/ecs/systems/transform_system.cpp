#include "transform_system.h"
#include <algorithm>

void TransformSystem::Update(float deltaTime) {
    (void)deltaTime;

    auto transforms = m_Registry->GetComponentArray<Transform>();

    // Get all root entities (no parent)
    std::vector<Entity> roots = m_Hierarchy->GetRootEntities();

    // Also check for entities with transforms but not in hierarchy
    for (size_t i = 0; i < transforms->Size(); ++i) {
        Entity entity = transforms->GetEntity(i);
        if (m_Hierarchy->GetParent(entity) == Entity::Invalid) {
            // This is a root - add if not already in list
            if (std::find(roots.begin(), roots.end(), entity) == roots.end()) {
                roots.push_back(entity);
            }
        }
    }

    // Update each root and its children recursively
    Mat4 identityMatrix(1.0f);
    for (Entity root : roots) {
        if (transforms->Has(root)) {
            UpdateTransformRecursive(root, identityMatrix);
        }
    }
}

void TransformSystem::UpdateTransformRecursive(Entity entity, const Mat4& parentWorld) {
    auto transforms = m_Registry->GetComponentArray<Transform>();

    if (!transforms->Has(entity)) {
        return;  // Entity doesn't have a transform
    }

    Transform& transform = transforms->Get(entity);

    // Compute local matrix from TRS
    Mat4 localMatrix = transform.GetLocalMatrix();

    // Compute world matrix
    transform.worldMatrix = parentWorld * localMatrix;
    transform.isDirty = false;

    // Update all children
    const auto& children = m_Hierarchy->GetChildren(entity);
    for (Entity child : children) {
        UpdateTransformRecursive(child, transform.worldMatrix);
    }
}

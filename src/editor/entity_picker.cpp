#include "entity_picker.h"
#include "ecs/components/transform.h"
#include "ecs/components/renderable.h"
#include "ecs/components/light.h"
#include "ecs/components/camera.h"
#include <limits>

EntityPicker::EntityPicker(ECSCoordinator* ecs)
    : m_ECS(ecs) {
}

EntityPicker::~EntityPicker() = default;

Entity EntityPicker::PickEntity(const Vec2& viewportPos, const Vec2& viewportSize,
                                const Mat4& viewMatrix, const Mat4& projMatrix) {
    // Create ray from screen position
    Ray ray = ScreenPointToRay(viewportPos, viewportSize, viewMatrix, projMatrix);

    Entity closestEntity = Entity::Invalid;
    f32 closestDistance = std::numeric_limits<f32>::max();

    // Test all entities with Transform
    // We want to be able to select any entity, even if it doesn't have a Renderable
    auto transforms = m_ECS->GetComponentRegistry()->GetComponentArray<Transform>();
    if (!transforms) {
        return Entity::Invalid;
    }

    for (size_t i = 0; i < transforms->Size(); ++i) {
        Entity entity = transforms->GetEntity(i);

        // Skip editor cameras
        if (m_ECS->HasComponent<Camera>(entity)) {
            const Camera& cam = m_ECS->GetComponent<Camera>(entity);
            if (cam.isEditorCamera) {
                continue;
            }
        }

        // Calculate bounds for this entity
        AABB bounds = CalculateEntityBounds(entity);

        if (!bounds.IsValid()) {
            continue;
        }

        // Test ray intersection
        f32 tMin, tMax;
        if (bounds.IntersectsRay(ray, tMin, tMax)) {
            // Use the entry distance for sorting
            if (tMin < closestDistance && tMin >= 0.0f) {
                closestDistance = tMin;
                closestEntity = entity;
            }
        }
    }

    return closestEntity;
}

AABB EntityPicker::CalculateEntityBounds(Entity entity) {
    if (!m_ECS->HasComponent<Transform>(entity)) {
        return AABB();  // Invalid bounds
    }

    const Transform& transform = m_ECS->GetComponent<Transform>(entity);

    // Extract world position from transform matrix
    Vec3 worldPos(transform.worldMatrix[3][0],
                  transform.worldMatrix[3][1],
                  transform.worldMatrix[3][2]);

    // Determine bounds based on component type
    AABB localBounds;

    if (m_ECS->HasComponent<Renderable>(entity)) {
        // For renderables, use a reasonable default size
        // TODO: In the future, get actual mesh bounds from MeshManager
        // For now, use scale-based bounds
        Vec3 extents = transform.localScale * 0.5f;
        localBounds = AABB::FromCenterExtents(Vec3(0, 0, 0), extents);
    } else if (m_ECS->HasComponent<Light>(entity)) {
        // Lights get a small gizmo-like bounds
        localBounds = AABB::FromCenterExtents(Vec3(0, 0, 0), Vec3(0.3f, 0.3f, 0.3f));
    } else if (m_ECS->HasComponent<Camera>(entity)) {
        // Cameras get a small gizmo-like bounds
        localBounds = AABB::FromCenterExtents(Vec3(0, 0, 0), Vec3(0.3f, 0.3f, 0.3f));
    } else {
        // Generic entity - small bounds for picking
        localBounds = AABB::FromCenterExtents(Vec3(0, 0, 0), Vec3(0.5f, 0.5f, 0.5f));
    }

    // Transform bounds to world space
    return localBounds.Transform(transform.worldMatrix);
}

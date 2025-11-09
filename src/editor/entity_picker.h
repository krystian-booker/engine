#pragma once

#include "ecs/ecs_coordinator.h"
#include "ecs/entity.h"
#include "core/ray.h"
#include "core/bounds.h"
#include "core/math.h"

/**
 * EntityPicker
 *
 * Handles entity selection via raycasting in the viewport.
 * Casts rays from screen space and finds intersecting entities.
 */
class EntityPicker {
public:
    EntityPicker(ECSCoordinator* ecs);
    ~EntityPicker();

    /**
     * Pick entity at viewport position
     *
     * @param viewportPos Mouse position in viewport (0,0 = top-left)
     * @param viewportSize Viewport dimensions
     * @param viewMatrix Camera view matrix
     * @param projMatrix Camera projection matrix
     * @return Picked entity, or Entity::Invalid if nothing hit
     */
    Entity PickEntity(const Vec2& viewportPos, const Vec2& viewportSize,
                     const Mat4& viewMatrix, const Mat4& projMatrix);

private:
    /**
     * Calculate axis-aligned bounding box for an entity
     *
     * @param entity Entity to calculate bounds for
     * @return AABB in world space
     */
    AABB CalculateEntityBounds(Entity entity);

    ECSCoordinator* m_ECS;
};

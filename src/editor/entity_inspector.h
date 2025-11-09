#pragma once

#include "ecs/ecs_coordinator.h"
#include "ecs/entity.h"

/**
 * EntityInspector
 *
 * Renders component property editors for selected entities.
 * Provides UI for viewing and editing all component properties.
 */
class EntityInspector {
public:
    EntityInspector(ECSCoordinator* ecs);
    ~EntityInspector();

    // Render inspector for an entity
    void Render(Entity entity);

private:
    // Component rendering methods
    void RenderNameComponent(Entity entity);
    void RenderTransformComponent(Entity entity);
    void RenderRenderableComponent(Entity entity);
    void RenderLightComponent(Entity entity);
    void RenderCameraComponent(Entity entity);
    void RenderRotatorComponent(Entity entity);

    // Add component menu
    void RenderAddComponentMenu(Entity entity);

    // Component header UI helpers
    bool BeginComponentHeader(const char* name, bool canRemove, bool* removeRequested);
    void EndComponentHeader();

    ECSCoordinator* m_ECS;
};

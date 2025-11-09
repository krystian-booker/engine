#pragma once

#include "ecs/entity.h"
#include <vector>

/**
 * EditorState
 *
 * Centralized state management for the editor.
 * Tracks selection, gizmo modes, and editor-specific state.
 */
class EditorState {
public:
    // Gizmo operation modes
    enum class GizmoMode {
        Translate,
        Rotate,
        Scale
    };

    // Gizmo coordinate space
    enum class GizmoSpace {
        Local,
        World
    };

    EditorState() = default;
    ~EditorState() = default;

    // Selection management
    Entity GetSelectedEntity() const { return m_SelectedEntity; }
    void SetSelectedEntity(Entity entity);
    void ClearSelection();
    bool HasSelection() const { return m_SelectedEntity.IsValid(); }

    // Multi-selection (for future use)
    const std::vector<Entity>& GetSelectedEntities() const { return m_SelectedEntities; }
    void AddToSelection(Entity entity);
    void RemoveFromSelection(Entity entity);
    bool IsSelected(Entity entity) const;

    // Viewport state
    u32 GetFocusedViewportID() const { return m_FocusedViewportID; }
    void SetFocusedViewportID(u32 id) { m_FocusedViewportID = id; }

    // Gizmo state
    GizmoMode GetGizmoMode() const { return m_GizmoMode; }
    void SetGizmoMode(GizmoMode mode) { m_GizmoMode = mode; }

    GizmoSpace GetGizmoSpace() const { return m_GizmoSpace; }
    void SetGizmoSpace(GizmoSpace space) { m_GizmoSpace = space; }

    // Hover state
    Entity GetHoveredEntity() const { return m_HoveredEntity; }
    void SetHoveredEntity(Entity entity) { m_HoveredEntity = entity; }

    // Manipulation state
    bool IsManipulating() const { return m_IsManipulating; }
    void SetManipulating(bool manipulating) { m_IsManipulating = manipulating; }

private:
    // Selection
    Entity m_SelectedEntity = Entity::Invalid;
    std::vector<Entity> m_SelectedEntities;

    // Hover
    Entity m_HoveredEntity = Entity::Invalid;

    // Viewport
    u32 m_FocusedViewportID = 0;

    // Gizmo
    GizmoMode m_GizmoMode = GizmoMode::Translate;
    GizmoSpace m_GizmoSpace = GizmoSpace::World;
    bool m_IsManipulating = false;
};

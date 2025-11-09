#include "editor_state.h"
#include <algorithm>

void EditorState::SetSelectedEntity(Entity entity) {
    m_SelectedEntity = entity;

    // Clear multi-selection and set to single selection
    m_SelectedEntities.clear();
    if (entity.IsValid()) {
        m_SelectedEntities.push_back(entity);
    }
}

void EditorState::ClearSelection() {
    m_SelectedEntity = Entity::Invalid;
    m_SelectedEntities.clear();
}

void EditorState::AddToSelection(Entity entity) {
    if (!entity.IsValid()) {
        return;
    }

    // Check if already selected
    auto it = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity);
    if (it == m_SelectedEntities.end()) {
        m_SelectedEntities.push_back(entity);
    }

    // Update primary selection
    if (!m_SelectedEntities.empty()) {
        m_SelectedEntity = m_SelectedEntities.back();
    }
}

void EditorState::RemoveFromSelection(Entity entity) {
    auto it = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity);
    if (it != m_SelectedEntities.end()) {
        m_SelectedEntities.erase(it);
    }

    // Update primary selection
    if (m_SelectedEntities.empty()) {
        m_SelectedEntity = Entity::Invalid;
    } else {
        m_SelectedEntity = m_SelectedEntities.back();
    }
}

bool EditorState::IsSelected(Entity entity) const {
    auto it = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity);
    return it != m_SelectedEntities.end();
}

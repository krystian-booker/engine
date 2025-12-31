#include "editor_state.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/components.hpp>
#include <algorithm>

namespace editor {

EditorState::EditorState(QObject* parent)
    : QObject(parent)
{
    m_undo_stack.setUndoLimit(100);
    m_scheduler = std::make_unique<engine::scene::Scheduler>();
    m_scheduler->add(engine::scene::Phase::PreRender,
                     engine::scene::transform_system, "transform", 0);
}

EditorState::~EditorState() = default;

void EditorState::set_world(World* world) {
    if (m_world != world) {
        m_world = world;
        clear_selection();
        emit world_changed();
    }
}

void EditorState::set_renderer(engine::render::IRenderer* renderer) {
    m_renderer = renderer;
}

void EditorState::select(Entity entity) {
    m_selection.clear();
    if (entity != engine::scene::NullEntity && m_world && m_world->valid(entity)) {
        m_selection.push_back(entity);
    }
    emit selection_changed();
}

void EditorState::add_to_selection(Entity entity) {
    if (entity == engine::scene::NullEntity || !m_world || !m_world->valid(entity)) {
        return;
    }
    if (!is_selected(entity)) {
        m_selection.push_back(entity);
        emit selection_changed();
    }
}

void EditorState::remove_from_selection(Entity entity) {
    auto it = std::find(m_selection.begin(), m_selection.end(), entity);
    if (it != m_selection.end()) {
        m_selection.erase(it);
        emit selection_changed();
    }
}

void EditorState::clear_selection() {
    if (!m_selection.empty()) {
        m_selection.clear();
        emit selection_changed();
    }
}

void EditorState::toggle_selection(Entity entity) {
    if (is_selected(entity)) {
        remove_from_selection(entity);
    } else {
        add_to_selection(entity);
    }
}

bool EditorState::is_selected(Entity entity) const {
    return std::find(m_selection.begin(), m_selection.end(), entity) != m_selection.end();
}

Entity EditorState::primary_selection() const {
    return m_selection.empty() ? engine::scene::NullEntity : m_selection.front();
}

void EditorState::set_mode(Mode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        emit mode_changed(mode);
    }
}

void EditorState::set_playing(bool playing) {
    if (m_playing != playing) {
        m_playing = playing;
        emit play_state_changed(playing);
    }
}

// CreateEntityCommand implementation
CreateEntityCommand::CreateEntityCommand(EditorState* state, const QString& name)
    : EditorCommand(state, "Create Entity")
    , m_name(name)
{
}

void CreateEntityCommand::undo() {
    if (m_state->world() && m_entity != engine::scene::NullEntity) {
        m_state->world()->destroy(m_entity);
        m_state->remove_from_selection(m_entity);
        m_entity = engine::scene::NullEntity;
    }
}

void CreateEntityCommand::redo() {
    if (m_state->world()) {
        m_entity = m_state->world()->create(m_name.toStdString());
        // Add default transform
        m_state->world()->emplace<engine::scene::LocalTransform>(m_entity);
        m_state->world()->emplace<engine::scene::WorldTransform>(m_entity);
        m_state->select(m_entity);
    }
}

// DeleteEntityCommand implementation
DeleteEntityCommand::DeleteEntityCommand(EditorState* state, Entity entity)
    : EditorCommand(state, "Delete Entity")
    , m_entity(entity)
{
    // TODO: Serialize entity state for undo
}

void DeleteEntityCommand::undo() {
    // TODO: Deserialize and recreate entity
}

void DeleteEntityCommand::redo() {
    if (m_state->world() && m_entity != engine::scene::NullEntity) {
        m_state->remove_from_selection(m_entity);
        m_state->world()->destroy(m_entity);
    }
}

// SetParentCommand implementation
SetParentCommand::SetParentCommand(EditorState* state,
                                   Entity child,
                                   Entity new_parent,
                                   std::optional<Entity> before_sibling)
    : EditorCommand(state, "Set Parent")
    , m_child(child)
    , m_old_parent(engine::scene::NullEntity)
    , m_new_parent(new_parent)
    , m_new_before_sibling(before_sibling)
{
    if (m_state->world() && m_state->world()->valid(child)) {
        auto* hier = m_state->world()->try_get<engine::scene::Hierarchy>(child);
        m_old_parent = hier ? hier->parent : engine::scene::NullEntity;
        if (hier) {
            m_old_before_sibling = hier->next_sibling;
        }
    }
}

void SetParentCommand::undo() {
    if (m_state->world() && m_state->world()->valid(m_child)) {
        if (m_old_before_sibling.has_value()) {
            engine::scene::set_parent(*m_state->world(), m_child, m_old_parent, *m_old_before_sibling);
        } else {
            engine::scene::set_parent(*m_state->world(), m_child, m_old_parent);
        }
    }
}

void SetParentCommand::redo() {
    if (m_state->world() && m_state->world()->valid(m_child)) {
        if (m_new_before_sibling.has_value()) {
            engine::scene::set_parent(*m_state->world(), m_child, m_new_parent, *m_new_before_sibling);
        } else {
            engine::scene::set_parent(*m_state->world(), m_child, m_new_parent);
        }
    }
}

} // namespace editor

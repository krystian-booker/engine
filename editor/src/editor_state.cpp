#include "editor_state.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/components.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/serialize.hpp>
#include <algorithm>

namespace editor {

EditorState::EditorState(QObject* parent)
    : QObject(parent)
{
    m_undo_stack.setUndoLimit(100);
    m_scheduler = std::make_unique<engine::scene::Scheduler>();
    m_scheduler->add(engine::scene::Phase::PreRender,
                     engine::scene::transform_system, "transform", 0);

    // Update active game camera when world changes
    connect(this, &EditorState::world_changed, this, &EditorState::update_active_game_camera);
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

void EditorState::update_active_game_camera() {
    if (!m_world) {
        if (m_active_game_camera != engine::scene::NullEntity) {
            m_active_game_camera = engine::scene::NullEntity;
            emit active_camera_changed(m_active_game_camera);
        }
        return;
    }

    Entity best = engine::scene::NullEntity;
    uint8_t best_priority = 0;

    // Scan all entities with Camera component
    auto view = m_world->view<engine::scene::Camera>();
    for (auto entity : view) {
        const auto& cam = view.get<engine::scene::Camera>(entity);
        if (cam.active) {
            // Higher priority wins, or first one found if priority is equal
            if (best == engine::scene::NullEntity || cam.priority > best_priority) {
                best = entity;
                best_priority = cam.priority;
            }
        }
    }

    // Only emit signal if camera actually changed
    if (best != m_active_game_camera) {
        m_active_game_camera = best;
        emit active_camera_changed(m_active_game_camera);
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

// RemoveComponentCommand implementation
RemoveComponentCommand::RemoveComponentCommand(EditorState* state, Entity entity, const std::string& type_name)
    : EditorCommand(state, QString("Remove %1").arg(QString::fromStdString(type_name)))
    , m_entity(entity)
    , m_type_name(type_name)
{
    // Serialize component data for potential undo
    if (m_state->world() && m_state->world()->valid(m_entity)) {
        auto& registry = engine::reflect::TypeRegistry::instance();
        auto comp_any = registry.get_component_any(m_state->world()->registry(), m_entity, m_type_name);
        if (comp_any) {
            // Serialize using JSON archive
            engine::core::JsonArchive ar;
            registry.serialize_any(comp_any, ar, "component");
            m_serialized_data = ar.to_string();
        }
    }
}

void RemoveComponentCommand::undo() {
    if (!m_state->world() || !m_state->world()->valid(m_entity)) return;

    auto& registry = engine::reflect::TypeRegistry::instance();

    // Re-add the component
    registry.add_component_any(m_state->world()->registry(), m_entity, m_type_name);

    // Restore serialized data
    if (!m_serialized_data.empty()) {
        engine::core::JsonArchive ar(m_serialized_data);
        auto type = registry.find_type(m_type_name);
        if (type) {
            auto restored = registry.deserialize_any(type, ar, "component");
            if (restored) {
                registry.set_component_any(m_state->world()->registry(), m_entity, m_type_name, restored);
            }
        }
    }
}

void RemoveComponentCommand::redo() {
    if (!m_state->world() || !m_state->world()->valid(m_entity)) return;

    engine::reflect::TypeRegistry::instance().remove_component_any(
        m_state->world()->registry(), m_entity, m_type_name);
}

} // namespace editor

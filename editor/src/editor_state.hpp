#pragma once

#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/render/renderer.hpp>
#include <engine/scene/systems.hpp>
#include <QObject>
#include <QUndoStack>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

namespace editor {

using engine::scene::Entity;
using engine::scene::World;

// Editor state management - selection, undo/redo, and engine integration
class EditorState : public QObject {
    Q_OBJECT

public:
    explicit EditorState(QObject* parent = nullptr);
    ~EditorState() override;

    // Scene management
    void set_world(World* world);
    World* world() const { return m_world; }

    // Renderer management
    void set_renderer(engine::render::IRenderer* renderer);
    engine::render::IRenderer* renderer() const { return m_renderer; }

    // System scheduler
    engine::scene::Scheduler* scheduler() const { return m_scheduler.get(); }

    // Selection management
    void select(Entity entity);
    void add_to_selection(Entity entity);
    void remove_from_selection(Entity entity);
    void clear_selection();
    void toggle_selection(Entity entity);

    const std::vector<Entity>& selection() const { return m_selection; }
    bool is_selected(Entity entity) const;
    Entity primary_selection() const;

    // Undo/Redo
    QUndoStack* undo_stack() { return &m_undo_stack; }

    // Editor modes
    enum class Mode { Select, Translate, Rotate, Scale };
    void set_mode(Mode mode);
    Mode mode() const { return m_mode; }

    // Grid settings
    float grid_snap() const { return m_grid_snap; }
    void set_grid_snap(float snap) { m_grid_snap = snap; }
    bool is_grid_enabled() const { return m_grid_enabled; }
    void set_grid_enabled(bool enabled) { m_grid_enabled = enabled; }

    // Play mode
    bool is_playing() const { return m_playing; }
    void set_playing(bool playing);

    // Active game camera tracking
    Entity active_game_camera() const { return m_active_game_camera; }
    void update_active_game_camera();

signals:
    void selection_changed();
    void mode_changed(Mode mode);
    void world_changed();
    void play_state_changed(bool playing);
    void active_camera_changed(engine::scene::Entity camera);

private:
    World* m_world = nullptr;
    engine::render::IRenderer* m_renderer = nullptr;
    std::vector<Entity> m_selection;
    QUndoStack m_undo_stack;
    Mode m_mode = Mode::Select;
    float m_grid_snap = 1.0f;
    bool m_grid_enabled = false;
    bool m_playing = false;
    std::unique_ptr<engine::scene::Scheduler> m_scheduler;
    Entity m_active_game_camera = engine::scene::NullEntity;
};

// Base command for undo/redo
class EditorCommand : public QUndoCommand {
public:
    EditorCommand(EditorState* state, const QString& text)
        : QUndoCommand(text), m_state(state) {}

protected:
    EditorState* m_state;
};

// Create entity command
class CreateEntityCommand : public EditorCommand {
public:
    CreateEntityCommand(EditorState* state, const QString& name = "Entity");

    void undo() override;
    void redo() override;

    Entity created_entity() const { return m_entity; }

private:
    QString m_name;
    Entity m_entity = engine::scene::NullEntity;
};

// Delete entity command
class DeleteEntityCommand : public EditorCommand {
public:
    DeleteEntityCommand(EditorState* state, Entity entity);

    void undo() override;
    void redo() override;

private:
    Entity m_entity;
    // Store serialized entity data for undo
    std::vector<uint8_t> m_serialized_data;
};

// Set parent command
class SetParentCommand : public EditorCommand {
public:
    SetParentCommand(EditorState* state,
                     Entity child,
                     Entity new_parent,
                     std::optional<Entity> before_sibling = std::nullopt);

    void undo() override;
    void redo() override;

private:
    Entity m_child;
    Entity m_old_parent;
    Entity m_new_parent;
    std::optional<Entity> m_old_before_sibling;
    std::optional<Entity> m_new_before_sibling;
};

// Remove component command
class RemoveComponentCommand : public EditorCommand {
public:
    RemoveComponentCommand(EditorState* state, Entity entity, const std::string& type_name);

    void undo() override;
    void redo() override;

private:
    Entity m_entity;
    std::string m_type_name;
    std::string m_serialized_data;  // JSON for undo restoration
};

} // namespace editor

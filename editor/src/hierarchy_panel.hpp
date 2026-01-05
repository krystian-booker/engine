#pragma once

#include "editor_state.hpp"
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>
#include <QDockWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace editor {

// Custom data roles for tree items
enum HierarchyDataRole {
    EntityIdRole = Qt::UserRole,
    VisibleInSceneRole = Qt::UserRole + 1,
    EnabledRole = Qt::UserRole + 2
};

// Forward declaration
class HierarchyPanel;

// Custom tree widget with drag-drop validation
class HierarchyTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit HierarchyTreeWidget(HierarchyPanel* panel, QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    bool is_valid_drop(QTreeWidgetItem* source, QTreeWidgetItem* target);
    bool is_asset_drop(const QMimeData* mime_data) const;
    void handle_asset_drop(const QMimeData* mime_data, QTreeWidgetItem* target);
    void clear_all_highlights();
    HierarchyPanel* m_panel;
};

// Hierarchy panel showing entity tree view with Unity-style features
class HierarchyPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit HierarchyPanel(EditorState* state, QWidget* parent = nullptr);
    ~HierarchyPanel() override;

    // Refresh the tree view
    void refresh();

    // Access for tree widget drag-drop validation
    EditorState* state() const { return m_state; }
    engine::scene::Entity entity_from_item(QTreeWidgetItem* item) const;

signals:
    void entity_selected(engine::scene::Entity entity);
    void entity_double_clicked(engine::scene::Entity entity);
    void frame_selection_requested();

private slots:
    void on_selection_changed();
    void on_item_selection_changed();
    void on_item_double_clicked(QTreeWidgetItem* item, int column);
    void on_item_renamed(QTreeWidgetItem* item, int column);
    void on_context_menu(const QPoint& pos);
    void on_search_text_changed(const QString& text);

    // Context menu actions - Basic
    void create_empty_entity();
    void create_child_entity();
    void delete_selected();
    void duplicate_selected();
    void rename_selected();

    // Context menu actions - Selection
    void select_children();
    void create_empty_parent();

    // Context menu actions - Clipboard
    void copy_selected();
    void paste_entities();
    void paste_as_child();

    // Context menu actions - Creation (3D objects, lights, etc.)
    void create_cube();
    void create_sphere();
    void create_plane();
    void create_cylinder();
    void create_directional_light();
    void create_point_light();
    void create_spot_light();
    void create_camera();
    void create_particle_system();

private:
    void setup_ui();
    void setup_connections();
    void setup_context_menu();
    void populate_tree();
    void add_entity_to_tree(engine::scene::Entity entity, QTreeWidgetItem* parent_item = nullptr);
    void update_entity_item(QTreeWidgetItem* item, engine::scene::Entity entity);

    QTreeWidgetItem* item_from_entity(engine::scene::Entity entity) const;

    // Search/filter
    void apply_filter(const QString& text);
    void mark_ancestors_visible(engine::scene::Entity entity);
    void expand_to_item(QTreeWidgetItem* item);

    // Keyboard event filter
    bool eventFilter(QObject* obj, QEvent* event) override;
    bool handle_key_press(QKeyEvent* event);

    EditorState* m_state;
    HierarchyTreeWidget* m_tree;
    QLineEdit* m_search_bar;
    QMenu* m_context_menu;

    // Entity-to-item mapping
    std::unordered_map<uint32_t, QTreeWidgetItem*> m_entity_items;

    // Filter state
    QString m_filter_text;
    std::unordered_set<uint32_t> m_visible_entities;

    // Clipboard for copy/paste - stores full component data
    struct ClipboardEntry {
        std::string name;
        std::optional<engine::scene::LocalTransform> transform;
        std::optional<engine::scene::Camera> camera;
        std::optional<engine::scene::Light> light;
        std::optional<engine::scene::MeshRenderer> mesh_renderer;
        std::optional<engine::scene::ParticleEmitter> particle;
    };
    std::vector<ClipboardEntry> m_clipboard;

    bool m_updating_selection = false;
};

} // namespace editor

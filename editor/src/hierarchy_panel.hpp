#pragma once

#include "editor_state.hpp"
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <QDockWidget>
#include <QTreeWidget>
#include <QMenu>
#include <unordered_map>

namespace editor {

// Hierarchy panel showing entity tree view
class HierarchyPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit HierarchyPanel(EditorState* state, QWidget* parent = nullptr);
    ~HierarchyPanel() override;

    // Refresh the tree view
    void refresh();

signals:
    void entity_selected(engine::scene::Entity entity);
    void entity_double_clicked(engine::scene::Entity entity);

private slots:
    void on_selection_changed();
    void on_item_selection_changed();
    void on_item_double_clicked(QTreeWidgetItem* item, int column);
    void on_context_menu(const QPoint& pos);

    // Context menu actions
    void create_empty_entity();
    void create_child_entity();
    void delete_selected();
    void duplicate_selected();
    void rename_selected();

private:
    void setup_ui();
    void setup_connections();
    void populate_tree();
    void add_entity_to_tree(engine::scene::Entity entity, QTreeWidgetItem* parent_item = nullptr);
    void update_entity_item(QTreeWidgetItem* item, engine::scene::Entity entity);

    engine::scene::Entity entity_from_item(QTreeWidgetItem* item) const;
    QTreeWidgetItem* item_from_entity(engine::scene::Entity entity) const;

    EditorState* m_state;
    QTreeWidget* m_tree;
    QMenu* m_context_menu;

    // Map entities to tree items for quick lookup
    std::unordered_map<uint32_t, QTreeWidgetItem*> m_entity_items;

    bool m_updating_selection = false;
};

} // namespace editor

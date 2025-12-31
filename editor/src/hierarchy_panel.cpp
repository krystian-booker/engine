#include "hierarchy_panel.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/components.hpp>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QInputDialog>

namespace editor {

HierarchyPanel::HierarchyPanel(EditorState* state, QWidget* parent)
    : QDockWidget("Hierarchy", parent)
    , m_state(state)
{
    setup_ui();
    setup_connections();
}

HierarchyPanel::~HierarchyPanel() = default;

void HierarchyPanel::setup_ui() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create tree widget
    m_tree = new QTreeWidget(container);
    m_tree->setHeaderLabel("Entities");
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);

    layout->addWidget(m_tree);
    setWidget(container);

    // Create context menu
    m_context_menu = new QMenu(this);
    m_context_menu->addAction("Create Empty", this, &HierarchyPanel::create_empty_entity);
    m_context_menu->addAction("Create Child", this, &HierarchyPanel::create_child_entity);
    m_context_menu->addSeparator();
    m_context_menu->addAction("Duplicate", this, &HierarchyPanel::duplicate_selected, QKeySequence("Ctrl+D"));
    m_context_menu->addAction("Delete", this, &HierarchyPanel::delete_selected, QKeySequence::Delete);
    m_context_menu->addSeparator();
    m_context_menu->addAction("Rename", this, &HierarchyPanel::rename_selected, QKeySequence("F2"));
}

void HierarchyPanel::setup_connections() {
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &HierarchyPanel::on_item_selection_changed);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &HierarchyPanel::on_item_double_clicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &HierarchyPanel::on_context_menu);

    if (m_state) {
        connect(m_state, &EditorState::selection_changed,
                this, &HierarchyPanel::on_selection_changed);
        connect(m_state, &EditorState::world_changed,
                this, &HierarchyPanel::refresh);
    }
}

void HierarchyPanel::refresh() {
    m_tree->clear();
    m_entity_items.clear();
    populate_tree();
}

void HierarchyPanel::populate_tree() {
    if (!m_state || !m_state->world()) {
        return;
    }

    auto* world = m_state->world();

    // Get all root entities (entities without parents)
    auto roots = engine::scene::get_root_entities(*world);

    for (auto entity : roots) {
        add_entity_to_tree(entity, nullptr);
    }

    m_tree->expandAll();
}

void HierarchyPanel::add_entity_to_tree(engine::scene::Entity entity, QTreeWidgetItem* parent_item) {
    if (!m_state || !m_state->world()) return;

    auto* world = m_state->world();
    if (!world->valid(entity)) return;

    // Create tree item
    QTreeWidgetItem* item;
    if (parent_item) {
        item = new QTreeWidgetItem(parent_item);
    } else {
        item = new QTreeWidgetItem(m_tree);
    }

    // Store entity ID in item data
    item->setData(0, Qt::UserRole, static_cast<uint32_t>(entity));
    item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

    // Update item display
    update_entity_item(item, entity);

    // Store mapping
    m_entity_items[static_cast<uint32_t>(entity)] = item;

    // Add children recursively
    auto* hierarchy = world->try_get<engine::scene::Hierarchy>(entity);
    if (hierarchy) {
        const auto& children = engine::scene::get_children(*world, entity);
        for (auto child : children) {
            add_entity_to_tree(child, item);
        }
    }
}

void HierarchyPanel::update_entity_item(QTreeWidgetItem* item, engine::scene::Entity entity) {
    if (!m_state || !m_state->world()) return;

    auto* world = m_state->world();
    auto* info = world->try_get<engine::scene::EntityInfo>(entity);

    QString name;
    if (info && !info->name.empty()) {
        name = QString::fromStdString(info->name);
    } else {
        name = QString("Entity %1").arg(static_cast<uint32_t>(entity));
    }

    item->setText(0, name);

    // Update icon based on components
    // TODO: Add icons for different component types
}

engine::scene::Entity HierarchyPanel::entity_from_item(QTreeWidgetItem* item) const {
    if (!item) return engine::scene::NullEntity;
    return static_cast<engine::scene::Entity>(item->data(0, Qt::UserRole).toUInt());
}

QTreeWidgetItem* HierarchyPanel::item_from_entity(engine::scene::Entity entity) const {
    auto it = m_entity_items.find(static_cast<uint32_t>(entity));
    return (it != m_entity_items.end()) ? it->second : nullptr;
}

void HierarchyPanel::on_selection_changed() {
    if (m_updating_selection) return;
    m_updating_selection = true;

    // Sync tree selection with editor selection
    m_tree->clearSelection();
    for (auto entity : m_state->selection()) {
        if (auto* item = item_from_entity(entity)) {
            item->setSelected(true);
        }
    }

    m_updating_selection = false;
}

void HierarchyPanel::on_item_selection_changed() {
    if (m_updating_selection) return;
    m_updating_selection = true;

    // Sync editor selection with tree selection
    m_state->clear_selection();
    for (auto* item : m_tree->selectedItems()) {
        auto entity = entity_from_item(item);
        if (entity != engine::scene::NullEntity) {
            m_state->add_to_selection(entity);
        }
    }

    m_updating_selection = false;
}

void HierarchyPanel::on_item_double_clicked(QTreeWidgetItem* item, int /*column*/) {
    auto entity = entity_from_item(item);
    if (entity != engine::scene::NullEntity) {
        emit entity_double_clicked(entity);
    }
}

void HierarchyPanel::on_context_menu(const QPoint& pos) {
    m_context_menu->exec(m_tree->mapToGlobal(pos));
}

void HierarchyPanel::create_empty_entity() {
    if (!m_state || !m_state->world()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Entity");
    m_state->undo_stack()->push(cmd);
    refresh();
}

void HierarchyPanel::create_child_entity() {
    if (!m_state || !m_state->world()) return;

    auto parent = m_state->primary_selection();
    if (parent == engine::scene::NullEntity) {
        create_empty_entity();
        return;
    }

    auto* cmd = new CreateEntityCommand(m_state, "Entity");
    m_state->undo_stack()->push(cmd);

    auto child = cmd->created_entity();
    if (child != engine::scene::NullEntity) {
        auto* parent_cmd = new SetParentCommand(m_state, child, parent);
        m_state->undo_stack()->push(parent_cmd);
    }

    refresh();
}

void HierarchyPanel::delete_selected() {
    if (!m_state || !m_state->world()) return;

    for (auto entity : m_state->selection()) {
        auto* cmd = new DeleteEntityCommand(m_state, entity);
        m_state->undo_stack()->push(cmd);
    }

    refresh();
}

void HierarchyPanel::duplicate_selected() {
    if (!m_state || !m_state->world()) return;

    // TODO: Implement entity duplication with all components
    refresh();
}

void HierarchyPanel::rename_selected() {
    auto items = m_tree->selectedItems();
    if (!items.isEmpty()) {
        m_tree->editItem(items.first(), 0);
    }
}

} // namespace editor

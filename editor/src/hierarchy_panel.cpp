#include "hierarchy_panel.hpp"
#include "entity_icons.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/components.hpp>
#include <engine/render/renderer.hpp>
#include <algorithm>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

namespace editor {

// ============================================================================
// HierarchyTreeWidget - Custom tree with drag-drop validation
// ============================================================================

HierarchyTreeWidget::HierarchyTreeWidget(HierarchyPanel* panel, QWidget* parent)
    : QTreeWidget(parent)
    , m_panel(panel)
{
}

void HierarchyTreeWidget::dragEnterEvent(QDragEnterEvent* event) {
    // Accept the drag if it's an internal move
    if (event->source() == this) {
        event->acceptProposedAction();
    } else {
        QTreeWidget::dragEnterEvent(event);
    }
}

void HierarchyTreeWidget::dragMoveEvent(QDragMoveEvent* event) {
    QTreeWidgetItem* target = itemAt(event->position().toPoint());

    // Clear previous highlighting
    for (int i = 0; i < topLevelItemCount(); ++i) {
        topLevelItem(i)->setBackground(0, QBrush());
    }

    // Validate drop for all selected items
    bool all_valid = true;
    for (auto* selected : selectedItems()) {
        if (!is_valid_drop(selected, target)) {
            all_valid = false;
            break;
        }
    }

    if (all_valid) {
        // Highlight valid drop target
        if (target) {
            target->setBackground(0, QBrush(QColor(100, 150, 200, 80)));
        }
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void HierarchyTreeWidget::dropEvent(QDropEvent* event) {
    QTreeWidgetItem* target = itemAt(event->position().toPoint());

    // Clear highlighting
    for (int i = 0; i < topLevelItemCount(); ++i) {
        topLevelItem(i)->setBackground(0, QBrush());
    }

    // Validate all selected items
    QList<QTreeWidgetItem*> items_to_move;
    for (auto* selected : selectedItems()) {
        if (is_valid_drop(selected, target)) {
            items_to_move.append(selected);
        }
    }

    if (items_to_move.isEmpty()) {
        event->ignore();
        return;
    }

    auto* state = m_panel->state();
    if (!state || !state->world()) {
        event->ignore();
        return;
    }

    // Determine drop type: OnItem (make child) vs AboveItem/BelowItem (sibling reorder)
    DropIndicatorPosition dropPos = dropIndicatorPosition();
    engine::scene::Entity new_parent = engine::scene::NullEntity;
    engine::scene::Entity before_sibling = engine::scene::NullEntity;

    if (dropPos == QAbstractItemView::OnItem) {
        // Dropping ON the item - make it a child of target (append at end)
        new_parent = m_panel->entity_from_item(target);
        before_sibling = engine::scene::NullEntity;
    } else if (target) {
        // Dropping above/below - make sibling (use target's parent)
        QTreeWidgetItem* parent_item = target->parent();
        if (parent_item) {
            new_parent = m_panel->entity_from_item(parent_item);
        }
        int target_index = parent_item ? parent_item->indexOfChild(target)
                                       : indexOfTopLevelItem(target);

        if (dropPos == QAbstractItemView::AboveItem) {
            before_sibling = m_panel->entity_from_item(target);
        } else if (dropPos == QAbstractItemView::BelowItem) {
            QTreeWidgetItem* next_item = parent_item
                ? parent_item->child(target_index + 1)
                : topLevelItem(target_index + 1);
            before_sibling = next_item ? m_panel->entity_from_item(next_item)
                                       : engine::scene::NullEntity;
        }
    }

    // Sort items in visual order to maintain relative ordering on multi-move
    std::vector<QTreeWidgetItem*> sorted_items(items_to_move.begin(), items_to_move.end());
    auto item_path = [](QTreeWidgetItem* item) {
        std::vector<int> path;
        while (item) {
            QTreeWidgetItem* parent = item->parent();
            int index = parent ? parent->indexOfChild(item)
                               : item->treeWidget()->indexOfTopLevelItem(item);
            path.push_back(index);
            item = parent;
        }
        std::reverse(path.begin(), path.end());
        return path;
    };
    std::sort(sorted_items.begin(), sorted_items.end(),
              [&](QTreeWidgetItem* a, QTreeWidgetItem* b) {
                  return item_path(a) < item_path(b);
              });

    // Use undo macro for batch reparenting
    state->undo_stack()->beginMacro("Reparent Entities");

    for (auto* item : sorted_items) {
        auto entity = m_panel->entity_from_item(item);
        if (entity != engine::scene::NullEntity) {
            auto* cmd = new SetParentCommand(state, entity, new_parent,
                                             std::optional<engine::scene::Entity>(before_sibling));
            state->undo_stack()->push(cmd);
        }
    }

    state->undo_stack()->endMacro();

    // Refresh the panel
    m_panel->refresh();

    event->acceptProposedAction();
}

bool HierarchyTreeWidget::is_valid_drop(QTreeWidgetItem* source, QTreeWidgetItem* target) {
    if (!source) return false;

    // Can always drop to root (target is null)
    if (!target) return true;

    // Cannot drop onto itself
    if (source == target) return false;

    // Cannot drop onto a descendant
    QTreeWidgetItem* parent = target->parent();
    while (parent) {
        if (parent == source) {
            return false; // Target is a descendant of source
        }
        parent = parent->parent();
    }

    // Additional validation using engine hierarchy
    auto* state = m_panel->state();
    if (state && state->world()) {
        auto source_entity = m_panel->entity_from_item(source);
        auto target_entity = m_panel->entity_from_item(target);

        if (source_entity != engine::scene::NullEntity &&
            target_entity != engine::scene::NullEntity) {
            // Use engine's is_ancestor_of check
            if (engine::scene::is_ancestor_of(*state->world(), source_entity, target_entity)) {
                return false;
            }
        }
    }

    return true;
}

HierarchyPanel::HierarchyPanel(EditorState* state, QWidget* parent)
    : QDockWidget("Hierarchy", parent)
    , m_state(state)
{
    EntityIcons::init();
    setup_ui();
    setup_context_menu();
    setup_connections();
}

HierarchyPanel::~HierarchyPanel() = default;

void HierarchyPanel::setup_ui() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Search bar
    m_search_bar = new QLineEdit(container);
    m_search_bar->setPlaceholderText("Search hierarchy...");
    m_search_bar->setClearButtonEnabled(true);
    layout->addWidget(m_search_bar);

    // Create custom tree widget with drag-drop validation
    m_tree = new HierarchyTreeWidget(this, container);
    m_tree->setHeaderLabel("Entities");
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setDefaultDropAction(Qt::MoveAction);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setIndentation(16);
    m_tree->setIconSize(QSize(16, 16));

    // Install event filter for keyboard navigation
    m_tree->installEventFilter(this);

    layout->addWidget(m_tree);
    setWidget(container);
}

void HierarchyPanel::setup_context_menu() {
    m_context_menu = new QMenu(this);

    // Basic creation
    m_context_menu->addAction("Create Empty", this, &HierarchyPanel::create_empty_entity);
    m_context_menu->addAction("Create Child", this, &HierarchyPanel::create_child_entity);

    m_context_menu->addSeparator();

    // 3D Object submenu
    auto* objects_menu = m_context_menu->addMenu("3D Object");
    objects_menu->setIcon(EntityIcons::mesh_icon());
    objects_menu->addAction("Cube", this, &HierarchyPanel::create_cube);
    objects_menu->addAction("Sphere", this, &HierarchyPanel::create_sphere);
    objects_menu->addAction("Plane", this, &HierarchyPanel::create_plane);
    objects_menu->addAction("Cylinder", this, &HierarchyPanel::create_cylinder);

    // Light submenu
    auto* light_menu = m_context_menu->addMenu("Light");
    light_menu->setIcon(EntityIcons::point_light_icon());
    light_menu->addAction(EntityIcons::directional_light_icon(), "Directional Light", this, &HierarchyPanel::create_directional_light);
    light_menu->addAction(EntityIcons::point_light_icon(), "Point Light", this, &HierarchyPanel::create_point_light);
    light_menu->addAction(EntityIcons::spot_light_icon(), "Spot Light", this, &HierarchyPanel::create_spot_light);

    // Camera
    m_context_menu->addAction(EntityIcons::camera_icon(), "Camera", this, &HierarchyPanel::create_camera);

    // Effects submenu
    auto* effects_menu = m_context_menu->addMenu("Effects");
    effects_menu->setIcon(EntityIcons::particle_icon());
    effects_menu->addAction(EntityIcons::particle_icon(), "Particle System", this, &HierarchyPanel::create_particle_system);

    m_context_menu->addSeparator();

    // Clipboard operations
    m_context_menu->addAction("Copy", this, &HierarchyPanel::copy_selected, QKeySequence::Copy);
    m_context_menu->addAction("Paste", this, &HierarchyPanel::paste_entities, QKeySequence::Paste);
    m_context_menu->addAction("Paste as Child", this, &HierarchyPanel::paste_as_child);

    m_context_menu->addSeparator();

    // Edit operations
    m_context_menu->addAction("Duplicate", this, &HierarchyPanel::duplicate_selected, QKeySequence("Ctrl+D"));
    m_context_menu->addAction("Delete", this, &HierarchyPanel::delete_selected, QKeySequence::Delete);
    m_context_menu->addAction("Rename", this, &HierarchyPanel::rename_selected, QKeySequence("F2"));

    m_context_menu->addSeparator();

    // Selection operations
    m_context_menu->addAction("Select Children", this, &HierarchyPanel::select_children);
    m_context_menu->addAction("Create Empty Parent", this, &HierarchyPanel::create_empty_parent);

    m_context_menu->addSeparator();

    // View operations
    m_context_menu->addAction("Expand All", m_tree, &QTreeWidget::expandAll);
    m_context_menu->addAction("Collapse All", m_tree, &QTreeWidget::collapseAll);
}

void HierarchyPanel::setup_connections() {
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &HierarchyPanel::on_item_selection_changed);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &HierarchyPanel::on_item_double_clicked);
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &HierarchyPanel::on_item_renamed);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &HierarchyPanel::on_context_menu);
    connect(m_search_bar, &QLineEdit::textChanged,
            this, &HierarchyPanel::on_search_text_changed);

    if (m_state) {
        connect(m_state, &EditorState::selection_changed,
                this, &HierarchyPanel::on_selection_changed);
        connect(m_state, &EditorState::world_changed,
                this, &HierarchyPanel::refresh);
    }
}

void HierarchyPanel::refresh() {
    // Block signals to prevent itemChanged triggering during rebuild
    m_tree->blockSignals(true);

    m_tree->clear();
    m_entity_items.clear();
    m_visible_entities.clear();
    populate_tree();

    // Reapply filter if active
    if (!m_filter_text.isEmpty()) {
        apply_filter(m_filter_text);
    }

    m_tree->blockSignals(false);
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
    item->setData(0, EntityIdRole, static_cast<uint32_t>(entity));
    item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

    // Update item display (name, icon, styling)
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

    // Set name
    QString name;
    if (info && !info->name.empty()) {
        name = QString::fromStdString(info->name);
    } else {
        name = QString("Entity %1").arg(static_cast<uint32_t>(entity));
    }
    item->setText(0, name);

    // Set icon based on components
    QIcon icon = EntityIcons::get_entity_icon(*world, entity);
    item->setIcon(0, icon);

    // Store enabled state and apply visual styling
    bool enabled = !info || info->enabled;
    item->setData(0, EnabledRole, enabled);

    // Dim disabled entities
    if (!enabled) {
        item->setForeground(0, QBrush(QColor(128, 128, 128)));
    } else {
        item->setForeground(0, QBrush(QColor(220, 220, 220)));
    }

    // Store visibility state (from MeshRenderer if present)
    bool visible = true;
    if (auto* mesh = world->try_get<engine::scene::MeshRenderer>(entity)) {
        visible = mesh->visible;
    }
    item->setData(0, VisibleInSceneRole, visible);
}

engine::scene::Entity HierarchyPanel::entity_from_item(QTreeWidgetItem* item) const {
    if (!item) return engine::scene::NullEntity;
    return static_cast<engine::scene::Entity>(item->data(0, EntityIdRole).toUInt());
}

QTreeWidgetItem* HierarchyPanel::item_from_entity(engine::scene::Entity entity) const {
    auto it = m_entity_items.find(static_cast<uint32_t>(entity));
    return (it != m_entity_items.end()) ? it->second : nullptr;
}

// ============================================================================
// Search & Filter
// ============================================================================

void HierarchyPanel::on_search_text_changed(const QString& text) {
    apply_filter(text);
}

void HierarchyPanel::apply_filter(const QString& text) {
    m_filter_text = text;
    m_visible_entities.clear();

    if (text.isEmpty()) {
        // Show all entities
        for (auto& [id, item] : m_entity_items) {
            item->setHidden(false);
        }
        return;
    }

    // First pass: find all matching entities
    std::vector<engine::scene::Entity> matches;
    for (auto& [id, item] : m_entity_items) {
        if (item->text(0).contains(text, Qt::CaseInsensitive)) {
            matches.push_back(static_cast<engine::scene::Entity>(id));
            m_visible_entities.insert(id);
        }
    }

    // Second pass: mark ancestors of matches as visible
    for (auto entity : matches) {
        mark_ancestors_visible(entity);
    }

    // Third pass: hide/show items
    for (auto& [id, item] : m_entity_items) {
        bool visible = m_visible_entities.count(id) > 0;
        item->setHidden(!visible);
        if (visible) {
            expand_to_item(item);
        }
    }
}

void HierarchyPanel::mark_ancestors_visible(engine::scene::Entity entity) {
    if (!m_state || !m_state->world()) return;

    auto* world = m_state->world();
    auto* hierarchy = world->try_get<engine::scene::Hierarchy>(entity);

    while (hierarchy && hierarchy->parent != engine::scene::NullEntity) {
        uint32_t parent_id = static_cast<uint32_t>(hierarchy->parent);
        if (m_visible_entities.count(parent_id) > 0) {
            break; // Already marked
        }
        m_visible_entities.insert(parent_id);
        hierarchy = world->try_get<engine::scene::Hierarchy>(hierarchy->parent);
    }
}

void HierarchyPanel::expand_to_item(QTreeWidgetItem* item) {
    QTreeWidgetItem* parent = item->parent();
    while (parent) {
        parent->setExpanded(true);
        parent = parent->parent();
    }
}

// ============================================================================
// Selection Handling
// ============================================================================

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

void HierarchyPanel::on_item_renamed(QTreeWidgetItem* item, int column) {
    if (column != 0) return;
    if (!m_state || !m_state->world()) return;

    auto entity = entity_from_item(item);
    if (entity == engine::scene::NullEntity) return;

    auto* world = m_state->world();
    auto* info = world->try_get<engine::scene::EntityInfo>(entity);
    if (info) {
        std::string new_name = item->text(0).toStdString();
        if (info->name != new_name) {
            info->name = new_name;
            // Force inspector to refresh by emitting selection_changed
            emit m_state->selection_changed();
        }
    }
}

void HierarchyPanel::on_context_menu(const QPoint& pos) {
    m_context_menu->exec(m_tree->mapToGlobal(pos));
}

// ============================================================================
// Keyboard Navigation
// ============================================================================

bool HierarchyPanel::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_tree && event->type() == QEvent::KeyPress) {
        auto* key_event = static_cast<QKeyEvent*>(event);
        if (handle_key_press(key_event)) {
            return true;
        }
    }
    return QDockWidget::eventFilter(obj, event);
}

bool HierarchyPanel::handle_key_press(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Delete:
            delete_selected();
            return true;

        case Qt::Key_F2:
            rename_selected();
            return true;

        case Qt::Key_D:
            if (event->modifiers() & Qt::ControlModifier) {
                duplicate_selected();
                return true;
            }
            break;

        case Qt::Key_C:
            if (event->modifiers() & Qt::ControlModifier) {
                copy_selected();
                return true;
            }
            break;

        case Qt::Key_V:
            if (event->modifiers() & Qt::ControlModifier) {
                if (event->modifiers() & Qt::ShiftModifier) {
                    paste_as_child();
                } else {
                    paste_entities();
                }
                return true;
            }
            break;

        case Qt::Key_A:
            if (event->modifiers() & Qt::ControlModifier) {
                m_tree->selectAll();
                return true;
            }
            break;

        case Qt::Key_F:
            emit frame_selection_requested();
            return true;

        case Qt::Key_Home:
            if (m_tree->topLevelItemCount() > 0) {
                m_tree->setCurrentItem(m_tree->topLevelItem(0));
            }
            return true;

        case Qt::Key_End: {
            // Navigate to last visible item
            int count = m_tree->topLevelItemCount();
            if (count > 0) {
                QTreeWidgetItem* last = m_tree->topLevelItem(count - 1);
                while (last->childCount() > 0 && last->isExpanded()) {
                    last = last->child(last->childCount() - 1);
                }
                m_tree->setCurrentItem(last);
            }
            return true;
        }

        case Qt::Key_Left:
            if (auto* item = m_tree->currentItem()) {
                if (item->isExpanded() && item->childCount() > 0) {
                    item->setExpanded(false);
                } else if (item->parent()) {
                    m_tree->setCurrentItem(item->parent());
                }
            }
            return true;

        case Qt::Key_Right:
            if (auto* item = m_tree->currentItem()) {
                if (item->childCount() > 0) {
                    if (!item->isExpanded()) {
                        item->setExpanded(true);
                    } else {
                        m_tree->setCurrentItem(item->child(0));
                    }
                }
            }
            return true;
    }
    return false;
}

// ============================================================================
// Entity Creation
// ============================================================================

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

// ============================================================================
// 3D Object Creation
// ============================================================================

void HierarchyPanel::create_cube() {
    if (!m_state || !m_state->world() || !m_state->renderer()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Cube");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        auto* renderer = m_state->renderer();

        engine::scene::MeshRenderer mr;
        mr.mesh = engine::scene::MeshHandle{renderer->create_primitive(engine::render::PrimitiveMesh::Cube).id};
        world->emplace<engine::scene::MeshRenderer>(entity, mr);
    }

    refresh();
}

void HierarchyPanel::create_sphere() {
    if (!m_state || !m_state->world() || !m_state->renderer()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Sphere");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        auto* renderer = m_state->renderer();

        engine::scene::MeshRenderer mr;
        mr.mesh = engine::scene::MeshHandle{renderer->create_primitive(engine::render::PrimitiveMesh::Sphere).id};
        world->emplace<engine::scene::MeshRenderer>(entity, mr);
    }

    refresh();
}

void HierarchyPanel::create_plane() {
    if (!m_state || !m_state->world() || !m_state->renderer()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Plane");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        auto* renderer = m_state->renderer();

        engine::scene::MeshRenderer mr;
        mr.mesh = engine::scene::MeshHandle{renderer->create_primitive(engine::render::PrimitiveMesh::Plane).id};
        world->emplace<engine::scene::MeshRenderer>(entity, mr);
    }

    refresh();
}

void HierarchyPanel::create_cylinder() {
    if (!m_state || !m_state->world() || !m_state->renderer()) return;

    // Note: Cylinder not available in PrimitiveMesh, using Quad as placeholder
    auto* cmd = new CreateEntityCommand(m_state, "Cylinder");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        auto* renderer = m_state->renderer();

        engine::scene::MeshRenderer mr;
        mr.mesh = engine::scene::MeshHandle{renderer->create_primitive(engine::render::PrimitiveMesh::Quad).id};
        world->emplace<engine::scene::MeshRenderer>(entity, mr);
    }

    refresh();
}

// ============================================================================
// Light Creation
// ============================================================================

void HierarchyPanel::create_directional_light() {
    if (!m_state || !m_state->world()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Directional Light");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        engine::scene::Light light;
        light.type = engine::scene::LightType::Directional;
        light.intensity = 1.0f;
        light.color = {1.0f, 1.0f, 1.0f};
        world->emplace<engine::scene::Light>(entity, light);
    }

    refresh();
}

void HierarchyPanel::create_point_light() {
    if (!m_state || !m_state->world()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Point Light");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        engine::scene::Light light;
        light.type = engine::scene::LightType::Point;
        light.intensity = 1.0f;
        light.range = 10.0f;
        light.color = {1.0f, 1.0f, 1.0f};
        world->emplace<engine::scene::Light>(entity, light);
    }

    refresh();
}

void HierarchyPanel::create_spot_light() {
    if (!m_state || !m_state->world()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Spot Light");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        engine::scene::Light light;
        light.type = engine::scene::LightType::Spot;
        light.intensity = 1.0f;
        light.range = 10.0f;
        light.spot_inner_angle = 30.0f;
        light.spot_outer_angle = 45.0f;
        light.color = {1.0f, 1.0f, 1.0f};
        world->emplace<engine::scene::Light>(entity, light);
    }

    refresh();
}

// ============================================================================
// Camera Creation
// ============================================================================

void HierarchyPanel::create_camera() {
    if (!m_state || !m_state->world()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Camera");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        engine::scene::Camera camera;
        camera.fov = 60.0f;
        camera.near_plane = 0.1f;
        camera.far_plane = 1000.0f;
        world->emplace<engine::scene::Camera>(entity, camera);
    }

    refresh();
}

// ============================================================================
// Effects Creation
// ============================================================================

void HierarchyPanel::create_particle_system() {
    if (!m_state || !m_state->world()) return;

    auto* cmd = new CreateEntityCommand(m_state, "Particle System");
    m_state->undo_stack()->push(cmd);
    auto entity = cmd->created_entity();

    if (entity != engine::scene::NullEntity) {
        auto* world = m_state->world();
        engine::scene::ParticleEmitter emitter;
        emitter.max_particles = 1000;
        emitter.emission_rate = 100.0f;
        emitter.lifetime = 2.0f;
        world->emplace<engine::scene::ParticleEmitter>(entity, emitter);
    }

    refresh();
}

// ============================================================================
// Entity Modification
// ============================================================================

void HierarchyPanel::delete_selected() {
    if (!m_state || !m_state->world()) return;

    auto selection = m_state->selection(); // Copy since we're modifying it
    for (auto entity : selection) {
        auto* cmd = new DeleteEntityCommand(m_state, entity);
        m_state->undo_stack()->push(cmd);
    }

    refresh();
}

void HierarchyPanel::duplicate_selected() {
    if (!m_state || !m_state->world()) return;

    auto* world = m_state->world();
    auto selection = m_state->selection();

    if (selection.empty()) return;

    m_state->undo_stack()->beginMacro("Duplicate Entities");

    std::vector<engine::scene::Entity> created;

    for (auto entity : selection) {
        // Skip if parent is also selected (will be duplicated with parent)
        auto* hier = world->try_get<engine::scene::Hierarchy>(entity);
        if (hier && hier->parent != engine::scene::NullEntity) {
            bool parent_selected = std::find(selection.begin(), selection.end(), hier->parent) != selection.end();
            if (parent_selected) continue;
        }

        // Get entity name
        auto* info = world->try_get<engine::scene::EntityInfo>(entity);
        QString name = info ? QString::fromStdString(info->name) : "Entity";

        // Create duplicate
        auto* cmd = new CreateEntityCommand(m_state, name);
        m_state->undo_stack()->push(cmd);
        auto duplicate = cmd->created_entity();

        if (duplicate == engine::scene::NullEntity) continue;

        // Copy transform
        if (auto* transform = world->try_get<engine::scene::LocalTransform>(entity)) {
            world->emplace_or_replace<engine::scene::LocalTransform>(duplicate, *transform);
        }

        // Copy components
        if (auto* mesh = world->try_get<engine::scene::MeshRenderer>(entity)) {
            world->emplace_or_replace<engine::scene::MeshRenderer>(duplicate, *mesh);
        }
        if (auto* camera = world->try_get<engine::scene::Camera>(entity)) {
            world->emplace_or_replace<engine::scene::Camera>(duplicate, *camera);
        }
        if (auto* light = world->try_get<engine::scene::Light>(entity)) {
            world->emplace_or_replace<engine::scene::Light>(duplicate, *light);
        }
        if (auto* particle = world->try_get<engine::scene::ParticleEmitter>(entity)) {
            world->emplace_or_replace<engine::scene::ParticleEmitter>(duplicate, *particle);
        }

        // Set same parent
        if (hier && hier->parent != engine::scene::NullEntity) {
            auto* parent_cmd = new SetParentCommand(m_state, duplicate, hier->parent);
            m_state->undo_stack()->push(parent_cmd);
        }

        created.push_back(duplicate);
    }

    m_state->undo_stack()->endMacro();

    // Select duplicated entities
    m_state->clear_selection();
    for (auto e : created) {
        m_state->add_to_selection(e);
    }

    refresh();
}

void HierarchyPanel::rename_selected() {
    auto items = m_tree->selectedItems();
    if (!items.isEmpty()) {
        m_tree->editItem(items.first(), 0);
    }
}

// ============================================================================
// Selection Operations
// ============================================================================

void HierarchyPanel::select_children() {
    if (!m_state || !m_state->world()) return;

    auto* world = m_state->world();
    auto selection = m_state->selection();

    for (auto entity : selection) {
        const auto& children = engine::scene::get_children(*world, entity);
        for (auto child : children) {
            m_state->add_to_selection(child);
        }
    }
}

void HierarchyPanel::create_empty_parent() {
    if (!m_state || !m_state->world()) return;

    auto selection = m_state->selection();
    if (selection.empty()) return;

    auto* world = m_state->world();

    // Find common parent of all selected entities
    engine::scene::Entity common_parent = engine::scene::NullEntity;
    bool all_same_parent = true;

    for (auto entity : selection) {
        auto* hier = world->try_get<engine::scene::Hierarchy>(entity);
        engine::scene::Entity parent = hier ? hier->parent : engine::scene::NullEntity;

        if (common_parent == engine::scene::NullEntity) {
            common_parent = parent;
        } else if (common_parent != parent) {
            all_same_parent = false;
            common_parent = engine::scene::NullEntity;
            break;
        }
    }

    m_state->undo_stack()->beginMacro("Create Empty Parent");

    // Create new parent entity
    auto* cmd = new CreateEntityCommand(m_state, "Parent");
    m_state->undo_stack()->push(cmd);
    auto new_parent = cmd->created_entity();

    if (new_parent == engine::scene::NullEntity) {
        m_state->undo_stack()->endMacro();
        return;
    }

    // Set new parent's parent to common parent (if any)
    if (common_parent != engine::scene::NullEntity) {
        auto* parent_cmd = new SetParentCommand(m_state, new_parent, common_parent);
        m_state->undo_stack()->push(parent_cmd);
    }

    // Reparent all selected entities to new parent
    for (auto entity : selection) {
        auto* reparent_cmd = new SetParentCommand(m_state, entity, new_parent);
        m_state->undo_stack()->push(reparent_cmd);
    }

    m_state->undo_stack()->endMacro();

    // Select the new parent
    m_state->clear_selection();
    m_state->select(new_parent);

    refresh();
}

// ============================================================================
// Clipboard Operations
// ============================================================================

void HierarchyPanel::copy_selected() {
    if (!m_state || !m_state->world()) return;

    auto* world = m_state->world();
    m_clipboard.clear();

    for (auto entity : m_state->selection()) {
        ClipboardEntry entry;

        // Copy name
        auto* info = world->try_get<engine::scene::EntityInfo>(entity);
        entry.name = info ? info->name : "Entity";

        // Copy transform
        if (auto* transform = world->try_get<engine::scene::LocalTransform>(entity)) {
            entry.transform = *transform;
        }

        // Copy components with full data
        if (auto* camera = world->try_get<engine::scene::Camera>(entity)) {
            entry.camera = *camera;
        }
        if (auto* light = world->try_get<engine::scene::Light>(entity)) {
            entry.light = *light;
        }
        if (auto* mesh = world->try_get<engine::scene::MeshRenderer>(entity)) {
            entry.mesh_renderer = *mesh;
        }
        if (auto* particle = world->try_get<engine::scene::ParticleEmitter>(entity)) {
            entry.particle = *particle;
        }

        m_clipboard.push_back(entry);
    }
}

void HierarchyPanel::paste_entities() {
    if (!m_state || !m_state->world() || m_clipboard.empty()) return;

    auto* world = m_state->world();
    m_state->undo_stack()->beginMacro("Paste Entities");

    std::vector<engine::scene::Entity> created;
    for (const auto& entry : m_clipboard) {
        auto* cmd = new CreateEntityCommand(m_state, QString::fromStdString(entry.name));
        m_state->undo_stack()->push(cmd);
        auto entity = cmd->created_entity();

        if (entity != engine::scene::NullEntity) {
            // Restore transform
            if (entry.transform) {
                world->emplace_or_replace<engine::scene::LocalTransform>(entity, *entry.transform);
            }

            // Restore components
            if (entry.camera) {
                world->emplace<engine::scene::Camera>(entity, *entry.camera);
            }
            if (entry.light) {
                world->emplace<engine::scene::Light>(entity, *entry.light);
            }
            if (entry.mesh_renderer) {
                world->emplace<engine::scene::MeshRenderer>(entity, *entry.mesh_renderer);
            }
            if (entry.particle) {
                world->emplace<engine::scene::ParticleEmitter>(entity, *entry.particle);
            }
        }

        created.push_back(entity);
    }

    m_state->undo_stack()->endMacro();

    // Select pasted entities
    m_state->clear_selection();
    for (auto e : created) {
        m_state->add_to_selection(e);
    }

    refresh();
}

void HierarchyPanel::paste_as_child() {
    if (!m_state || !m_state->world() || m_clipboard.empty()) return;

    auto parent = m_state->primary_selection();
    if (parent == engine::scene::NullEntity) {
        paste_entities();
        return;
    }

    auto* world = m_state->world();
    m_state->undo_stack()->beginMacro("Paste as Child");

    std::vector<engine::scene::Entity> created;
    for (const auto& entry : m_clipboard) {
        auto* cmd = new CreateEntityCommand(m_state, QString::fromStdString(entry.name));
        m_state->undo_stack()->push(cmd);
        auto entity = cmd->created_entity();

        if (entity != engine::scene::NullEntity) {
            // Set parent
            auto* parent_cmd = new SetParentCommand(m_state, entity, parent);
            m_state->undo_stack()->push(parent_cmd);

            // Restore transform
            if (entry.transform) {
                world->emplace_or_replace<engine::scene::LocalTransform>(entity, *entry.transform);
            }

            // Restore components
            if (entry.camera) {
                world->emplace<engine::scene::Camera>(entity, *entry.camera);
            }
            if (entry.light) {
                world->emplace<engine::scene::Light>(entity, *entry.light);
            }
            if (entry.mesh_renderer) {
                world->emplace<engine::scene::MeshRenderer>(entity, *entry.mesh_renderer);
            }
            if (entry.particle) {
                world->emplace<engine::scene::ParticleEmitter>(entity, *entry.particle);
            }
        }

        created.push_back(entity);
    }

    m_state->undo_stack()->endMacro();

    // Select pasted entities
    m_state->clear_selection();
    for (auto e : created) {
        m_state->add_to_selection(e);
    }

    refresh();
}

} // namespace editor

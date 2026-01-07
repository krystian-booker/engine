#include "inspector_panel.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/scene/components.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/serialize.hpp>
#include <set>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QComboBox>
#include <QGroupBox>
#include <QFrame>
#include <QSizePolicy>
#include <QMenu>
#include <QContextMenuEvent>
#include <QListWidget>
#include <QDialogButtonBox>

namespace editor {

// CollapsibleSection implementation
CollapsibleSection::CollapsibleSection(const QString& title, QWidget* content, QWidget* parent)
    : QWidget(parent)
    , m_content(content)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header frame with dark styling
    m_header = new QFrame(this);
    m_header->setFrameShape(QFrame::NoFrame);
    m_header->setStyleSheet(
        "QFrame { background-color: #3C3C3C; border: 1px solid #555; border-radius: 2px; }"
    );

    auto* header_layout = new QHBoxLayout(m_header);
    header_layout->setContentsMargins(4, 4, 4, 4);
    header_layout->setSpacing(4);

    // Arrow indicator
    m_arrow_label = new QLabel(m_header);
    m_arrow_label->setFixedSize(12, 12);
    m_arrow_label->setAlignment(Qt::AlignCenter);
    m_arrow_label->setStyleSheet("QLabel { color: #AAA; font-size: 10px; border: none; background: transparent; }");
    update_arrow();
    header_layout->addWidget(m_arrow_label);

    // Title button
    m_toggle_btn = new QPushButton(title, m_header);
    m_toggle_btn->setFlat(true);
    m_toggle_btn->setStyleSheet(
        "QPushButton { text-align: left; font-weight: bold; color: #DDD; border: none; background: transparent; }"
        "QPushButton:hover { color: #FFF; }"
    );
    header_layout->addWidget(m_toggle_btn, 1);

    // Options menu button
    auto* menu_btn = new QPushButton("...", m_header);
    menu_btn->setFixedSize(20, 16);
    menu_btn->setFlat(true);
    menu_btn->setStyleSheet(
        "QPushButton { color: #888; border: none; background: transparent; font-weight: bold; }"
        "QPushButton:hover { color: #FFF; background: #555; border-radius: 2px; }"
    );
    connect(menu_btn, &QPushButton::clicked, [this, menu_btn]() {
        show_context_menu(menu_btn->mapToGlobal(QPoint(0, menu_btn->height())));
    });
    header_layout->addWidget(menu_btn);

    layout->addWidget(m_header);
    layout->addWidget(m_content);

    connect(m_toggle_btn, &QPushButton::clicked, [this]() {
        set_collapsed(!m_collapsed);
    });

    // Also toggle when clicking the arrow
    m_arrow_label->installEventFilter(this);
}

void CollapsibleSection::set_collapsed(bool collapsed) {
    m_collapsed = collapsed;
    m_content->setVisible(!collapsed);
    update_arrow();
}

void CollapsibleSection::update_arrow() {
    // Unicode arrows: right arrow for collapsed, down arrow for expanded
    m_arrow_label->setText(m_collapsed ? "\u25B6" : "\u25BC");
}

void CollapsibleSection::contextMenuEvent(QContextMenuEvent* event) {
    show_context_menu(event->globalPos());
}

void CollapsibleSection::show_context_menu(const QPoint& pos) {
    QMenu menu(this);

    menu.addAction("Reset", [this]() { emit reset_requested(); });
    menu.addSeparator();
    menu.addAction("Copy Component", [this]() { emit copy_requested(); });
    menu.addAction("Paste Component Values", [this]() { emit paste_requested(); });

    if (m_removable) {
        menu.addSeparator();
        menu.addAction("Remove Component", [this]() { emit remove_requested(); });
    }

    menu.exec(pos);
}

// DraggableLabel implementation
DraggableLabel::DraggableLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent)
{
    setCursor(Qt::SizeHorCursor);
}

void DraggableLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_drag_start = event->globalPosition().toPoint();
        emit drag_started();
        grabMouse();
    }
    QLabel::mousePressEvent(event);
}

void DraggableLabel::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        int delta_x = event->globalPosition().toPoint().x() - m_drag_start.x();
        double value_delta = delta_x * m_sensitivity;
        emit value_changed(value_delta);
        m_drag_start = event->globalPosition().toPoint();
    }
    QLabel::mouseMoveEvent(event);
}

void DraggableLabel::mouseReleaseEvent(QMouseEvent* event) {
    if (m_dragging) {
        m_dragging = false;
        releaseMouse();
        emit drag_finished();
    }
    QLabel::mouseReleaseEvent(event);
}

void DraggableLabel::enterEvent(QEnterEvent* event) {
    QLabel::enterEvent(event);
}

void DraggableLabel::leaveEvent(QEvent* event) {
    QLabel::leaveEvent(event);
}

// AddComponentDialog implementation
AddComponentDialog::AddComponentDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Add Component");
    setMinimumSize(280, 350);
    setup_ui();
}

void AddComponentDialog::setup_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Search box
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Search components...");
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    // Component list
    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);

    // Get all registered components from TypeRegistry
    auto& registry = engine::reflect::TypeRegistry::instance();
    auto component_names = registry.get_all_component_names();

    // Fallback if TypeRegistry is empty (static initialization order issue)
    if (component_names.empty()) {
        component_names = {
            "MeshRenderer", "Camera", "Light", "Skybox", "Billboard",
            "ParticleEmitter", "AudioSource", "AudioListener", "AudioTrigger",
            "RigidBodyComponent", "BuoyancyComponent", "VehicleComponent", "ClothComponent",
            "ScriptComponent", "NavAgentComponent", "NavObstacleComponent",
            "BlendShapeComponent", "IKComponent", "DecalComponent",
            "WaterSurfaceComponent", "GrassComponent", "FoliageComponent",
            "UICanvasComponent", "UIWorldCanvasComponent",
            "InteractableComponent", "InteractionHighlightComponent"
        };
    }

    for (const auto& name : component_names) {
        // Skip internal/system components that shouldn't be added manually
        if (name == "EntityInfo" || name == "Hierarchy" ||
            name == "LocalTransform" || name == "WorldTransform" ||
            name == "PreviousTransform") {
            continue;
        }

        auto* type_info = registry.get_type_info(name);
        QString display_name;
        if (type_info && !type_info->meta.display_name.empty()) {
            display_name = QString::fromStdString(type_info->meta.display_name);
        } else {
            // Use the type name directly as display name
            display_name = QString::fromStdString(name);
        }

        auto* item = new QListWidgetItem(display_name);
        item->setData(Qt::UserRole, QString::fromStdString(name));  // Store actual type name
        m_list->addItem(item);
    }

    // Sort alphabetically
    m_list->sortItems();

    layout->addWidget(m_list, 1);

    // Buttons
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    // Connections
    connect(m_search, &QLineEdit::textChanged, this, &AddComponentDialog::filter_components);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Select first item by default
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
}

QString AddComponentDialog::selected_component() const {
    auto* item = m_list->currentItem();
    if (!item) return QString();

    // Return the actual type name stored in UserRole, not the display text
    QVariant data = item->data(Qt::UserRole);
    return data.isValid() ? data.toString() : item->text();
}

void AddComponentDialog::filter_components(const QString& text) {
    for (int i = 0; i < m_list->count(); ++i) {
        auto* item = m_list->item(i);
        bool matches = item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }

    // Select first visible item
    for (int i = 0; i < m_list->count(); ++i) {
        auto* item = m_list->item(i);
        if (!item->isHidden()) {
            m_list->setCurrentItem(item);
            break;
        }
    }
}

// InspectorPanel implementation
InspectorPanel::InspectorPanel(EditorState* state, QWidget* parent)
    : QDockWidget("Inspector", parent)
    , m_state(state)
{
    setup_ui();

    if (m_state) {
        connect(m_state, &EditorState::selection_changed,
                this, &InspectorPanel::on_selection_changed);
    }
}

InspectorPanel::~InspectorPanel() = default;

void InspectorPanel::setup_ui() {
    m_scroll_area = new QScrollArea(this);
    m_scroll_area->setWidgetResizable(true);
    m_scroll_area->setFrameShape(QFrame::NoFrame);

    m_content = new QWidget(m_scroll_area);
    m_layout = new QVBoxLayout(m_content);
    m_layout->setAlignment(Qt::AlignTop);
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(8);

    m_scroll_area->setWidget(m_content);
    setWidget(m_scroll_area);
}

void InspectorPanel::refresh() {
    on_selection_changed();
}

void InspectorPanel::clear_content() {
    while (QLayoutItem* item = m_layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();  // Use deleteLater() for safe Qt widget deletion
        }
        delete item;
    }
    m_euler_cache.clear();
}

void InspectorPanel::on_selection_changed() {
    clear_content();

    if (!m_state || m_state->selection().empty()) {
        auto* label = new QLabel("No entity selected", m_content);
        label->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(label);
        return;
    }

    auto entity = m_state->primary_selection();
    show_entity_info(entity);
}

void InspectorPanel::show_entity_info(engine::scene::Entity entity) {
    if (!m_state || !m_state->world()) return;

    auto* world = m_state->world();
    if (!world->valid(entity)) return;

    // Entity name and enabled
    auto* info = world->try_get<engine::scene::EntityInfo>(entity);
    if (info) {
        auto* name_edit = new QLineEdit(QString::fromStdString(info->name), m_content);
        connect(name_edit, &QLineEdit::textChanged, [info](const QString& text) {
            info->name = text.toStdString();
        });

        auto* enabled_check = new QCheckBox("Enabled", m_content);
        enabled_check->setChecked(info->enabled);
        connect(enabled_check, &QCheckBox::toggled, [info](bool checked) {
            info->enabled = checked;
        });

        auto* header = new QWidget(m_content);
        auto* header_layout = new QHBoxLayout(header);
        header_layout->setContentsMargins(0, 0, 0, 0);
        header_layout->addWidget(name_edit);
        header_layout->addWidget(enabled_check);
        m_layout->addWidget(header);
    }

    // Transform component - specialized editor
    if (world->has<engine::scene::LocalTransform>(entity)) {
        auto* transform_widget = create_transform_editor(entity);
        add_component_section("Transform", transform_widget, "LocalTransform");
    }

    // Components with specialized editors
    std::set<std::string> specialized_components = {
        "EntityInfo", "LocalTransform", "WorldTransform", "PreviousTransform", "Hierarchy"
    };

    // MeshRenderer - specialized editor
    if (world->has<engine::scene::MeshRenderer>(entity)) {
        auto* mesh_widget = create_mesh_renderer_editor(entity);
        add_component_section("Mesh Renderer", mesh_widget, "MeshRenderer");
        specialized_components.insert("MeshRenderer");
    }

    // Camera - specialized editor
    if (world->has<engine::scene::Camera>(entity)) {
        auto* camera_widget = create_camera_editor(entity);
        add_component_section("Camera", camera_widget, "Camera");
        specialized_components.insert("Camera");
    }

    // Light - specialized editor
    if (world->has<engine::scene::Light>(entity)) {
        auto* light_widget = create_light_editor(entity);
        add_component_section("Light", light_widget, "Light");
        specialized_components.insert("Light");
    }

    // Display all other components using generic editor
    auto& registry = engine::reflect::TypeRegistry::instance();
    auto component_names = registry.get_all_component_names();

    for (const auto& type_name : component_names) {
        // Skip components with specialized editors or internal components
        if (specialized_components.count(type_name) > 0) {
            continue;
        }

        // Check if entity has this component
        auto comp_any = registry.get_component_any(world->registry(), entity, type_name);
        if (comp_any) {
            auto* type_info = registry.get_type_info(type_name);
            QString display_name;
            if (type_info && !type_info->meta.display_name.empty()) {
                display_name = QString::fromStdString(type_info->meta.display_name);
            } else {
                display_name = QString::fromStdString(type_name);
            }

            auto* widget = create_generic_component_editor(entity, type_name);
            add_component_section(display_name, widget, type_name);
        }
    }

    // Add Component button
    auto* add_btn = new QPushButton("Add Component", m_content);
    connect(add_btn, &QPushButton::clicked, this, &InspectorPanel::on_add_component);
    m_layout->addWidget(add_btn);

    // Paste as New Component button (only shown when clipboard has content)
    if (m_component_clipboard.has_value()) {
        auto* paste_btn = new QPushButton(
            QString("Paste %1").arg(QString::fromStdString(m_component_clipboard->type_name)),
            m_content);
        paste_btn->setStyleSheet("QPushButton { color: #7799FF; }");
        connect(paste_btn, &QPushButton::clicked, this, &InspectorPanel::on_paste_as_new_component);
        m_layout->addWidget(paste_btn);
    }

    // Spacer
    m_layout->addStretch();
}

void InspectorPanel::add_component_section(const QString& title, QWidget* content, const std::string& type_name) {
    auto* section = new CollapsibleSection(title, content, m_content);
    section->set_component_type(type_name);

    // Disable removal for core transform components
    if (type_name == "LocalTransform" || type_name == "WorldTransform") {
        section->set_removable(false);
    }

    m_layout->addWidget(section);

    // Connect context menu signals
    connect(section, &CollapsibleSection::remove_requested, this, [this, section]() {
        on_remove_component(section);
    });
    connect(section, &CollapsibleSection::copy_requested, this, [this, section]() {
        on_copy_component(section);
    });
    connect(section, &CollapsibleSection::paste_requested, this, [this, section]() {
        on_paste_component(section);
    });
    connect(section, &CollapsibleSection::reset_requested, this, [this, section]() {
        on_reset_component(section);
    });
}

QWidget* InspectorPanel::create_transform_editor(engine::scene::Entity entity) {
    auto* world = m_state->world();
    auto& transform = world->get<engine::scene::LocalTransform>(entity);

    auto* widget = new QWidget(m_content);
    auto* layout = new QFormLayout(widget);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setVerticalSpacing(4);

    // Position
    auto* pos_widget = create_vec3_editor("Position", transform.position, []{});
    layout->addRow("Position", pos_widget);

    // Rotation (euler angles) - use cache to avoid static variable issues
    auto entity_id = static_cast<uint32_t>(entity);
    m_euler_cache[entity_id] = glm::degrees(transform.euler());
    auto& euler = m_euler_cache[entity_id];
    auto* rot_widget = create_vec3_editor("Rotation", euler, [&transform, &euler]() {
        transform.set_euler(glm::radians(euler));
    });
    layout->addRow("Rotation", rot_widget);

    // Scale
    auto* scale_widget = create_vec3_editor("Scale", transform.scale, []{});
    layout->addRow("Scale", scale_widget);

    return widget;
}

QWidget* InspectorPanel::create_mesh_renderer_editor(engine::scene::Entity entity) {
    auto* world = m_state->world();
    auto& renderer = world->get<engine::scene::MeshRenderer>(entity);

    auto* widget = new QWidget(m_content);
    auto* layout = new QFormLayout(widget);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setVerticalSpacing(4);

    // Visible checkbox
    auto* visible_cb = new QCheckBox(widget);
    visible_cb->setChecked(renderer.visible);
    connect(visible_cb, &QCheckBox::toggled, [&renderer](bool checked) {
        renderer.visible = checked;
    });
    layout->addRow("Visible", visible_cb);

    // Mesh handle (read-only for now)
    auto* mesh_label = new QLabel(QString("Mesh: %1").arg(renderer.mesh.id), widget);
    layout->addRow(mesh_label);

    // Material handle (read-only for now)
    auto* mat_label = new QLabel(QString("Material: %1").arg(renderer.material.id), widget);
    layout->addRow(mat_label);

    return widget;
}

QWidget* InspectorPanel::create_camera_editor(engine::scene::Entity entity) {
    auto* world = m_state->world();
    auto& camera = world->get<engine::scene::Camera>(entity);

    auto* widget = new QWidget(m_content);
    auto* layout = new QFormLayout(widget);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setVerticalSpacing(4);

    // FOV
    auto* fov_spin = new QDoubleSpinBox(widget);
    fov_spin->setRange(1.0, 180.0);
    fov_spin->setValue(camera.fov);
    fov_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    fov_spin->setMaximumWidth(80);
    connect(fov_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&camera](double value) { camera.fov = static_cast<float>(value); });
    layout->addRow("FOV", fov_spin);

    // Near plane
    auto* near_spin = new QDoubleSpinBox(widget);
    near_spin->setRange(0.001, 1000.0);
    near_spin->setDecimals(3);
    near_spin->setValue(camera.near_plane);
    near_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    near_spin->setMaximumWidth(80);
    connect(near_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&camera](double value) { camera.near_plane = static_cast<float>(value); });
    layout->addRow("Near", near_spin);

    // Far plane
    auto* far_spin = new QDoubleSpinBox(widget);
    far_spin->setRange(1.0, 100000.0);
    far_spin->setValue(camera.far_plane);
    far_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    far_spin->setMaximumWidth(80);
    connect(far_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&camera](double value) { camera.far_plane = static_cast<float>(value); });
    layout->addRow("Far", far_spin);

    // Active checkbox
    auto* active_cb = new QCheckBox(widget);
    active_cb->setChecked(camera.active);
    connect(active_cb, &QCheckBox::toggled, [&camera](bool checked) {
        camera.active = checked;
    });
    layout->addRow("Active", active_cb);

    return widget;
}

QWidget* InspectorPanel::create_light_editor(engine::scene::Entity entity) {
    auto* world = m_state->world();
    auto& light = world->get<engine::scene::Light>(entity);

    auto* widget = new QWidget(m_content);
    auto* layout = new QFormLayout(widget);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setVerticalSpacing(4);

    // Type
    auto* type_combo = new QComboBox(widget);
    type_combo->addItem("Directional");
    type_combo->addItem("Point");
    type_combo->addItem("Spot");
    type_combo->setCurrentIndex(static_cast<int>(light.type));
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [&light](int index) { light.type = static_cast<engine::scene::LightType>(index); });
    layout->addRow("Type", type_combo);

    // Color
    auto* color_widget = create_color_editor("Color", light.color, []{});
    layout->addRow("Color", color_widget);

    // Intensity
    auto* intensity_spin = new QDoubleSpinBox(widget);
    intensity_spin->setRange(0.0, 100.0);
    intensity_spin->setValue(light.intensity);
    intensity_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    intensity_spin->setMaximumWidth(80);
    connect(intensity_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&light](double value) { light.intensity = static_cast<float>(value); });
    layout->addRow("Intensity", intensity_spin);

    // Range (for point/spot)
    auto* range_spin = new QDoubleSpinBox(widget);
    range_spin->setRange(0.0, 1000.0);
    range_spin->setValue(light.range);
    range_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    range_spin->setMaximumWidth(80);
    connect(range_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&light](double value) { light.range = static_cast<float>(value); });
    layout->addRow("Range", range_spin);

    // Spot angle (for spot only)
    auto* angle_spin = new QDoubleSpinBox(widget);
    angle_spin->setRange(1.0, 180.0);
    angle_spin->setValue(light.spot_outer_angle);
    angle_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    angle_spin->setMaximumWidth(80);
    connect(angle_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&light](double value) { light.spot_outer_angle = static_cast<float>(value); });
    layout->addRow("Spot Angle", angle_spin);

    return widget;
}

QWidget* InspectorPanel::create_generic_component_editor(engine::scene::Entity entity, const std::string& type_name) {
    auto* widget = new QWidget(m_content);
    auto* layout = new QFormLayout(widget);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setVerticalSpacing(4);

    auto& registry = engine::reflect::TypeRegistry::instance();
    auto* type_info = registry.get_type_info(type_name);

    if (!type_info || type_info->properties.empty()) {
        auto* label = new QLabel("No editable properties", widget);
        label->setStyleSheet("color: #888;");
        layout->addRow(label);
        return widget;
    }

    // Get the component as meta_any for property access
    auto comp_any = registry.get_component_any(m_state->world()->registry(), entity, type_name);
    if (!comp_any) {
        return widget;
    }

    for (const auto& prop : type_info->properties) {
        // Skip hidden properties
        if (prop.meta.hidden) {
            continue;
        }

        QString prop_display_name;
        if (!prop.meta.display_name.empty()) {
            prop_display_name = QString::fromStdString(prop.meta.display_name);
        } else {
            prop_display_name = QString::fromStdString(prop.name);
        }

        // Get current value
        auto value_any = prop.getter(comp_any);
        if (!value_any) {
            continue;
        }

        // Create appropriate editor based on type
        QWidget* editor = nullptr;

        // Bool
        if (auto* bool_val = value_any.try_cast<bool>()) {
            auto* cb = new QCheckBox(widget);
            cb->setChecked(*bool_val);
            connect(cb, &QCheckBox::toggled, [this, entity, type_name, prop_name = prop.name](bool checked) {
                auto& reg = engine::reflect::TypeRegistry::instance();
                auto comp = reg.get_component_any(m_state->world()->registry(), entity, type_name);
                if (comp) {
                    auto* prop_info = reg.get_property_info(type_name, prop_name);
                    if (prop_info) {
                        prop_info->setter(comp, entt::meta_any{checked});
                        reg.set_component_any(m_state->world()->registry(), entity, type_name, comp);
                    }
                }
            });
            editor = cb;
        }
        // Float
        else if (auto* float_val = value_any.try_cast<float>()) {
            auto* spin = new QDoubleSpinBox(widget);
            spin->setRange(prop.meta.min_value, prop.meta.max_value);
            spin->setDecimals(2);
            spin->setValue(*float_val);
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            spin->setMaximumWidth(80);
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    [this, entity, type_name, prop_name = prop.name](double val) {
                auto& reg = engine::reflect::TypeRegistry::instance();
                auto comp = reg.get_component_any(m_state->world()->registry(), entity, type_name);
                if (comp) {
                    auto* prop_info = reg.get_property_info(type_name, prop_name);
                    if (prop_info) {
                        prop_info->setter(comp, entt::meta_any{static_cast<float>(val)});
                        reg.set_component_any(m_state->world()->registry(), entity, type_name, comp);
                    }
                }
            });
            editor = spin;
        }
        // Int32
        else if (auto* int_val = value_any.try_cast<int32_t>()) {
            auto* spin = new QSpinBox(widget);
            spin->setRange(static_cast<int>(prop.meta.min_value), static_cast<int>(prop.meta.max_value));
            spin->setValue(*int_val);
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            spin->setMaximumWidth(80);
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                    [this, entity, type_name, prop_name = prop.name](int val) {
                auto& reg = engine::reflect::TypeRegistry::instance();
                auto comp = reg.get_component_any(m_state->world()->registry(), entity, type_name);
                if (comp) {
                    auto* prop_info = reg.get_property_info(type_name, prop_name);
                    if (prop_info) {
                        prop_info->setter(comp, entt::meta_any{static_cast<int32_t>(val)});
                        reg.set_component_any(m_state->world()->registry(), entity, type_name, comp);
                    }
                }
            });
            editor = spin;
        }
        // String
        else if (auto* str_val = value_any.try_cast<std::string>()) {
            auto* edit = new QLineEdit(QString::fromStdString(*str_val), widget);
            connect(edit, &QLineEdit::textChanged,
                    [this, entity, type_name, prop_name = prop.name](const QString& text) {
                auto& reg = engine::reflect::TypeRegistry::instance();
                auto comp = reg.get_component_any(m_state->world()->registry(), entity, type_name);
                if (comp) {
                    auto* prop_info = reg.get_property_info(type_name, prop_name);
                    if (prop_info) {
                        prop_info->setter(comp, entt::meta_any{text.toStdString()});
                        reg.set_component_any(m_state->world()->registry(), entity, type_name, comp);
                    }
                }
            });
            editor = edit;
        }
        // Vec3
        else if (auto* vec3_val = value_any.try_cast<engine::core::Vec3>()) {
            // For Vec3 we just show a read-only label for now
            // Full editing would require storing the property path
            auto* label = new QLabel(QString("(%1, %2, %3)")
                .arg(vec3_val->x, 0, 'f', 2)
                .arg(vec3_val->y, 0, 'f', 2)
                .arg(vec3_val->z, 0, 'f', 2), widget);
            label->setStyleSheet("color: #AAA;");
            editor = label;
        }
        // Default - show type and value info
        else {
            auto* label = new QLabel("(unsupported type)", widget);
            label->setStyleSheet("color: #666;");
            editor = label;
        }

        if (editor) {
            layout->addRow(prop_display_name, editor);
        }
    }

    return widget;
}

QWidget* InspectorPanel::create_vec3_editor(const QString& /*label*/, engine::core::Vec3& value,
                                             std::function<void()> on_changed) {
    auto* widget = new QWidget(m_content);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Helper to create a component with draggable label + spinbox
    auto create_component = [&](float& v, const QString& axis, const QString& color) {
        auto* container = new QWidget(widget);
        auto* h_layout = new QHBoxLayout(container);
        h_layout->setContentsMargins(0, 0, 0, 0);
        h_layout->setSpacing(1);

        // Draggable axis label
        auto* label = new DraggableLabel(axis, container);
        label->setFixedWidth(14);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; }").arg(color));
        label->set_sensitivity(0.1);

        auto* spin = new QDoubleSpinBox(container);
        spin->setRange(-100000.0, 100000.0);
        spin->setDecimals(2);
        spin->setValue(v);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setMinimumWidth(45);
        spin->setMaximumWidth(55);
        spin->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        // Connect draggable label to adjust value
        connect(label, &DraggableLabel::value_changed, [spin, &v, on_changed](double delta) {
            v += static_cast<float>(delta);
            spin->setValue(v);
            if (on_changed) on_changed();
        });

        // Connect spinbox value changes
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [&v, on_changed](double val) {
                    v = static_cast<float>(val);
                    if (on_changed) on_changed();
                });

        h_layout->addWidget(label);
        h_layout->addWidget(spin);
        return container;
    };

    // Color-coded X (red), Y (green), Z (blue) - Unity style
    layout->addWidget(create_component(value.x, "X", "#FF6464"));
    layout->addWidget(create_component(value.y, "Y", "#64FF64"));
    layout->addWidget(create_component(value.z, "Z", "#6496FF"));
    layout->addStretch();

    return widget;
}

QWidget* InspectorPanel::create_float_editor(const QString& /*label*/, float& value,
                                              std::function<void()> on_changed,
                                              float min, float max) {
    auto* spin = new QDoubleSpinBox(m_content);
    spin->setRange(min, max);
    spin->setDecimals(2);
    spin->setValue(value);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setMaximumWidth(80);
    spin->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&value, on_changed](double val) {
                value = static_cast<float>(val);
                if (on_changed) on_changed();
            });
    return spin;
}

QWidget* InspectorPanel::create_bool_editor(const QString& /*label*/, bool& value,
                                             std::function<void()> on_changed) {
    auto* cb = new QCheckBox(m_content);
    cb->setChecked(value);
    connect(cb, &QCheckBox::toggled, [&value, on_changed](bool checked) {
        value = checked;
        if (on_changed) on_changed();
    });
    return cb;
}

QWidget* InspectorPanel::create_color_editor(const QString& /*label*/, engine::core::Vec3& value,
                                              std::function<void()> on_changed) {
    auto* btn = new QPushButton(m_content);
    auto update_color = [btn, &value]() {
        QColor c = QColor::fromRgbF(value.x, value.y, value.z);
        btn->setStyleSheet(QString("background-color: %1").arg(c.name()));
    };
    update_color();

    connect(btn, &QPushButton::clicked, [btn, &value, on_changed, update_color]() {
        QColor initial = QColor::fromRgbF(value.x, value.y, value.z);
        QColor c = QColorDialog::getColor(initial, btn, "Select Color");
        if (c.isValid()) {
            value.x = static_cast<float>(c.redF());
            value.y = static_cast<float>(c.greenF());
            value.z = static_cast<float>(c.blueF());
            update_color();
            if (on_changed) on_changed();
        }
    });

    return btn;
}

void InspectorPanel::on_add_component() {
    if (!m_state || m_state->selection().empty()) return;

    AddComponentDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString component = dialog.selected_component();
        if (component.isEmpty()) return;

        auto entity = m_state->primary_selection();
        auto* world = m_state->world();
        if (!world || !world->valid(entity)) return;

        // Add the selected component using TypeRegistry
        auto& registry = engine::reflect::TypeRegistry::instance();
        std::string type_name = component.toStdString();

        // Check if entity already has this component
        auto comp_any = registry.get_component_any(world->registry(), entity, type_name);
        if (!comp_any) {
            registry.add_component_any(world->registry(), entity, type_name);
        }

        // Refresh the inspector to show the new component
        refresh();
    }
}

void InspectorPanel::on_remove_component(CollapsibleSection* section) {
    if (!m_state || m_state->selection().empty()) return;

    auto entity = m_state->primary_selection();
    const std::string& type_name = section->component_type();

    if (type_name.empty()) return;

    // Prevent removing Transform (core component required for all entities)
    if (type_name == "LocalTransform" || type_name == "WorldTransform") {
        return;
    }

    auto* cmd = new RemoveComponentCommand(m_state, entity, type_name);
    m_state->undo_stack()->push(cmd);

    refresh();
}

void InspectorPanel::on_copy_component(CollapsibleSection* section) {
    if (!m_state || m_state->selection().empty()) return;

    auto entity = m_state->primary_selection();
    const std::string& type_name = section->component_type();
    if (type_name.empty()) return;

    auto& registry = engine::reflect::TypeRegistry::instance();
    auto comp_any = registry.get_component_any(m_state->world()->registry(), entity, type_name);

    if (comp_any) {
        ComponentClipboard clip;
        clip.type_name = type_name;

        // Serialize to JSON
        engine::core::JsonArchive ar;
        registry.serialize_any(comp_any, ar, "component");
        clip.serialized_json = ar.to_string();

        m_component_clipboard = std::move(clip);
    }
}

void InspectorPanel::on_paste_component(CollapsibleSection* section) {
    if (!m_state || m_state->selection().empty()) return;
    if (!m_component_clipboard.has_value()) return;

    auto entity = m_state->primary_selection();
    const std::string& target_type = section->component_type();
    const std::string& source_type = m_component_clipboard->type_name;

    // Only paste if component types match
    if (target_type != source_type) return;

    auto& registry = engine::reflect::TypeRegistry::instance();

    // Deserialize and apply
    engine::core::JsonArchive ar(m_component_clipboard->serialized_json);
    auto type = registry.find_type(source_type);
    if (type) {
        auto restored = registry.deserialize_any(type, ar, "component");
        if (restored) {
            registry.set_component_any(m_state->world()->registry(), entity, source_type, restored);
        }
    }

    refresh();
}

void InspectorPanel::on_reset_component(CollapsibleSection* section) {
    if (!m_state || m_state->selection().empty()) return;

    auto entity = m_state->primary_selection();
    const std::string& type_name = section->component_type();
    if (type_name.empty()) return;

    auto& registry = engine::reflect::TypeRegistry::instance();

    // Create a default instance
    auto type = registry.find_type(type_name);
    if (type) {
        auto default_instance = type.construct();
        if (default_instance) {
            registry.set_component_any(m_state->world()->registry(), entity, type_name, default_instance);
        }
    }

    refresh();
}

void InspectorPanel::on_paste_as_new_component() {
    if (!m_state || m_state->selection().empty()) return;
    if (!m_component_clipboard.has_value()) return;

    auto entity = m_state->primary_selection();
    auto& registry = engine::reflect::TypeRegistry::instance();
    const std::string& source_type = m_component_clipboard->type_name;

    // Add the component if it doesn't exist
    auto existing = registry.get_component_any(m_state->world()->registry(), entity, source_type);
    if (!existing) {
        registry.add_component_any(m_state->world()->registry(), entity, source_type);
    }

    // Paste the values
    engine::core::JsonArchive ar(m_component_clipboard->serialized_json);
    auto type = registry.find_type(source_type);
    if (type) {
        auto restored = registry.deserialize_any(type, ar, "component");
        if (restored) {
            registry.set_component_any(m_state->world()->registry(), entity, source_type, restored);
        }
    }

    refresh();
}

} // namespace editor

#include "inspector_panel.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/scene/components.hpp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QComboBox>
#include <QGroupBox>
#include <QFrame>

namespace editor {

// CollapsibleSection implementation
CollapsibleSection::CollapsibleSection(const QString& title, QWidget* content, QWidget* parent)
    : QWidget(parent)
    , m_content(content)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Header with toggle button
    auto* header = new QFrame(this);
    header->setFrameShape(QFrame::StyledPanel);
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(4, 2, 4, 2);

    m_toggle_btn = new QPushButton(title, header);
    m_toggle_btn->setFlat(true);
    m_toggle_btn->setCheckable(true);
    m_toggle_btn->setStyleSheet("text-align: left; font-weight: bold;");
    header_layout->addWidget(m_toggle_btn);
    header_layout->addStretch();

    layout->addWidget(header);
    layout->addWidget(m_content);

    connect(m_toggle_btn, &QPushButton::toggled, [this](bool checked) {
        set_collapsed(checked);
    });
}

void CollapsibleSection::set_collapsed(bool collapsed) {
    m_collapsed = collapsed;
    m_content->setVisible(!collapsed);
    m_toggle_btn->setText(m_toggle_btn->text()); // Refresh
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
            delete item->widget();
        }
        delete item;
    }
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

    // Transform component
    if (world->has<engine::scene::LocalTransform>(entity)) {
        auto* transform_widget = create_transform_editor(entity);
        add_component_section("Transform", transform_widget);
    }

    // MeshRenderer component
    if (world->has<engine::scene::MeshRenderer>(entity)) {
        auto* mesh_widget = create_mesh_renderer_editor(entity);
        add_component_section("Mesh Renderer", mesh_widget);
    }

    // Camera component
    if (world->has<engine::scene::Camera>(entity)) {
        auto* camera_widget = create_camera_editor(entity);
        add_component_section("Camera", camera_widget);
    }

    // Light component
    if (world->has<engine::scene::Light>(entity)) {
        auto* light_widget = create_light_editor(entity);
        add_component_section("Light", light_widget);
    }

    // Add Component button
    auto* add_btn = new QPushButton("Add Component", m_content);
    connect(add_btn, &QPushButton::clicked, this, &InspectorPanel::on_add_component);
    m_layout->addWidget(add_btn);

    // Spacer
    m_layout->addStretch();
}

void InspectorPanel::add_component_section(const QString& title, QWidget* content) {
    auto* section = new CollapsibleSection(title, content, m_content);
    m_layout->addWidget(section);
}

QWidget* InspectorPanel::create_transform_editor(engine::scene::Entity entity) {
    auto* world = m_state->world();
    auto& transform = world->get<engine::scene::LocalTransform>(entity);

    auto* widget = new QWidget(m_content);
    auto* layout = new QFormLayout(widget);
    layout->setContentsMargins(8, 4, 8, 4);

    // Position
    auto* pos_widget = create_vec3_editor("Position", transform.position, []{});
    layout->addRow("Position", pos_widget);

    // Rotation (euler angles)
    static engine::core::Vec3 euler;
    euler = transform.euler();
    euler = glm::degrees(euler);
    auto* rot_widget = create_vec3_editor("Rotation", euler, [&transform]() {
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
    layout->setContentsMargins(8, 4, 8, 4);

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
    layout->setContentsMargins(8, 4, 8, 4);

    // FOV
    auto* fov_spin = new QDoubleSpinBox(widget);
    fov_spin->setRange(1.0, 180.0);
    fov_spin->setValue(camera.fov);
    connect(fov_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&camera](double value) { camera.fov = static_cast<float>(value); });
    layout->addRow("FOV", fov_spin);

    // Near plane
    auto* near_spin = new QDoubleSpinBox(widget);
    near_spin->setRange(0.001, 1000.0);
    near_spin->setDecimals(3);
    near_spin->setValue(camera.near_plane);
    connect(near_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&camera](double value) { camera.near_plane = static_cast<float>(value); });
    layout->addRow("Near", near_spin);

    // Far plane
    auto* far_spin = new QDoubleSpinBox(widget);
    far_spin->setRange(1.0, 100000.0);
    far_spin->setValue(camera.far_plane);
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
    layout->setContentsMargins(8, 4, 8, 4);

    // Type
    auto* type_combo = new QComboBox(widget);
    type_combo->addItem("Directional");
    type_combo->addItem("Point");
    type_combo->addItem("Spot");
    type_combo->setCurrentIndex(static_cast<int>(light.type));
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [&light](int index) { light.type = static_cast<engine::scene::Light::Type>(index); });
    layout->addRow("Type", type_combo);

    // Color
    auto* color_widget = create_color_editor("Color", light.color, []{});
    layout->addRow("Color", color_widget);

    // Intensity
    auto* intensity_spin = new QDoubleSpinBox(widget);
    intensity_spin->setRange(0.0, 100.0);
    intensity_spin->setValue(light.intensity);
    connect(intensity_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&light](double value) { light.intensity = static_cast<float>(value); });
    layout->addRow("Intensity", intensity_spin);

    // Range (for point/spot)
    auto* range_spin = new QDoubleSpinBox(widget);
    range_spin->setRange(0.0, 1000.0);
    range_spin->setValue(light.range);
    connect(range_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&light](double value) { light.range = static_cast<float>(value); });
    layout->addRow("Range", range_spin);

    // Spot angle (for spot only)
    auto* angle_spin = new QDoubleSpinBox(widget);
    angle_spin->setRange(1.0, 180.0);
    angle_spin->setValue(light.spot_angle);
    connect(angle_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&light](double value) { light.spot_angle = static_cast<float>(value); });
    layout->addRow("Spot Angle", angle_spin);

    return widget;
}

QWidget* InspectorPanel::create_vec3_editor(const QString& /*label*/, engine::core::Vec3& value,
                                             std::function<void()> on_changed) {
    auto* widget = new QWidget(m_content);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto create_spin = [&](float& v, const QString& prefix) {
        auto* spin = new QDoubleSpinBox(widget);
        spin->setPrefix(prefix + ": ");
        spin->setRange(-100000.0, 100000.0);
        spin->setDecimals(3);
        spin->setValue(v);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [&v, on_changed](double val) {
                    v = static_cast<float>(val);
                    if (on_changed) on_changed();
                });
        return spin;
    };

    layout->addWidget(create_spin(value.x, "X"));
    layout->addWidget(create_spin(value.y, "Y"));
    layout->addWidget(create_spin(value.z, "Z"));

    return widget;
}

QWidget* InspectorPanel::create_float_editor(const QString& /*label*/, float& value,
                                              std::function<void()> on_changed,
                                              float min, float max) {
    auto* spin = new QDoubleSpinBox(m_content);
    spin->setRange(min, max);
    spin->setDecimals(3);
    spin->setValue(value);
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
    // TODO: Show component picker dialog
}

} // namespace editor

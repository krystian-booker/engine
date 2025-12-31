#pragma once

#include "editor_state.hpp"
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <QDockWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QWidget>
#include <functional>
#include <memory>

namespace editor {

// Forward declarations for property editors
class PropertyEditor;

// Inspector panel showing component properties for selected entity
class InspectorPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit InspectorPanel(EditorState* state, QWidget* parent = nullptr);
    ~InspectorPanel() override;

    // Refresh to show currently selected entity
    void refresh();

private slots:
    void on_selection_changed();
    void on_add_component();

private:
    void setup_ui();
    void clear_content();
    void show_entity_info(engine::scene::Entity entity);
    void add_component_section(const QString& title, QWidget* content);

    // Component editors
    QWidget* create_transform_editor(engine::scene::Entity entity);
    QWidget* create_mesh_renderer_editor(engine::scene::Entity entity);
    QWidget* create_camera_editor(engine::scene::Entity entity);
    QWidget* create_light_editor(engine::scene::Entity entity);

    // Property widgets
    QWidget* create_vec3_editor(const QString& label, engine::core::Vec3& value,
                                 std::function<void()> on_changed);
    QWidget* create_float_editor(const QString& label, float& value,
                                  std::function<void()> on_changed,
                                  float min = -FLT_MAX, float max = FLT_MAX);
    QWidget* create_bool_editor(const QString& label, bool& value,
                                 std::function<void()> on_changed);
    QWidget* create_color_editor(const QString& label, engine::core::Vec3& value,
                                  std::function<void()> on_changed);

    EditorState* m_state;
    QScrollArea* m_scroll_area;
    QWidget* m_content;
    QVBoxLayout* m_layout;
};

// Collapsible section for component groups
class CollapsibleSection : public QWidget {
    Q_OBJECT

public:
    explicit CollapsibleSection(const QString& title, QWidget* content, QWidget* parent = nullptr);

    void set_collapsed(bool collapsed);
    bool is_collapsed() const { return m_collapsed; }

private:
    bool m_collapsed = false;
    QWidget* m_content;
    class QPushButton* m_toggle_btn;
};

} // namespace editor

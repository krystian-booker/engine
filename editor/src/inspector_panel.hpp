#pragma once

#include "editor_state.hpp"
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <QDockWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QMouseEvent>
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

class QContextMenuEvent;

namespace editor {

// Forward declarations
class PropertyEditor;
class CollapsibleSection;

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
    void on_remove_component(CollapsibleSection* section);
    void on_copy_component(CollapsibleSection* section);
    void on_paste_component(CollapsibleSection* section);
    void on_reset_component(CollapsibleSection* section);
    void on_paste_as_new_component();

private:
    void setup_ui();
    void clear_content();
    void show_entity_info(engine::scene::Entity entity);
    void add_component_section(const QString& title, QWidget* content, const std::string& type_name);

    // Component editors
    QWidget* create_transform_editor(engine::scene::Entity entity);
    QWidget* create_mesh_renderer_editor(engine::scene::Entity entity);
    QWidget* create_camera_editor(engine::scene::Entity entity);
    QWidget* create_light_editor(engine::scene::Entity entity);
    QWidget* create_generic_component_editor(engine::scene::Entity entity, const std::string& type_name);

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

    // Cache for euler angles (avoids static variable issues)
    std::unordered_map<uint32_t, engine::core::Vec3> m_euler_cache;

    // Component clipboard for copy/paste
    struct ComponentClipboard {
        std::string type_name;
        std::string serialized_json;
    };
    std::optional<ComponentClipboard> m_component_clipboard;
};

// Collapsible section for component groups
class CollapsibleSection : public QWidget {
    Q_OBJECT

public:
    explicit CollapsibleSection(const QString& title, QWidget* content, QWidget* parent = nullptr);

    void set_collapsed(bool collapsed);
    bool is_collapsed() const { return m_collapsed; }
    void set_removable(bool removable) { m_removable = removable; }

    // Component type for reflection-based operations
    void set_component_type(const std::string& type_name) { m_component_type = type_name; }
    const std::string& component_type() const { return m_component_type; }

signals:
    void remove_requested();
    void reset_requested();
    void copy_requested();
    void paste_requested();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void update_arrow();
    void show_context_menu(const QPoint& pos);

    bool m_collapsed = false;
    bool m_removable = true;
    QWidget* m_content;
    QPushButton* m_toggle_btn;
    QLabel* m_arrow_label;
    QFrame* m_header;
    std::string m_component_type;
};

// Dialog for adding components to an entity
class AddComponentDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddComponentDialog(QWidget* parent = nullptr);

    QString selected_component() const;

private:
    void setup_ui();
    void filter_components(const QString& text);

    QLineEdit* m_search;
    QListWidget* m_list;
};

// Draggable label that adjusts a value when dragged horizontally (Unity-style)
class DraggableLabel : public QLabel {
    Q_OBJECT

public:
    explicit DraggableLabel(const QString& text, QWidget* parent = nullptr);

    void set_sensitivity(double sensitivity) { m_sensitivity = sensitivity; }
    double sensitivity() const { return m_sensitivity; }

signals:
    void value_changed(double delta);
    void drag_started();
    void drag_finished();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool m_dragging = false;
    QPoint m_drag_start;
    double m_sensitivity = 0.1;
};

} // namespace editor

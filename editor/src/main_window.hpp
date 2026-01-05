#pragma once

#include "editor_state.hpp"
#include <engine/scene/world.hpp>
#include <engine/render/renderer.hpp>
#include <QMainWindow>
#include <QShowEvent>
#include <QTabWidget>
#include <memory>

namespace editor {

class ViewportWidget;
class GameViewWidget;
class HierarchyPanel;
class InspectorPanel;
class AssetBrowser;
class ConsolePanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    // File menu
    void on_new_project();
    void on_open_project();
    void on_save_project();
    void on_save_project_as();

    // Edit menu
    void on_undo();
    void on_redo();
    void on_duplicate();
    void on_delete();

    // Create menu
    void on_create_empty_entity();
    void on_create_cube();
    void on_create_sphere();
    void on_create_camera();
    void on_create_directional_light();
    void on_create_point_light();

    // View menu
    void on_toggle_hierarchy();
    void on_toggle_inspector();
    void on_toggle_assets();
    void on_toggle_console();
    void on_reset_layout();
    void on_switch_to_scene_view();
    void on_switch_to_game_view();

    // Play controls
    void on_play();
    void on_pause();
    void on_stop();

    // Viewport
    void on_entity_picked(engine::scene::Entity entity);
    void on_viewport_resized(int width, int height);

private:
    void setup_ui();
    void setup_menus();
    void setup_toolbar();
    void setup_panels();
    void setup_connections();

    void create_demo_scene();
    void init_engine();
    void shutdown_engine();

    void save_layout();
    void restore_layout();

    // Engine components
    std::unique_ptr<EditorState> m_state;
    std::unique_ptr<engine::scene::World> m_world;
    std::unique_ptr<engine::render::IRenderer> m_renderer;

    // Central tabbed widget
    QTabWidget* m_central_tabs = nullptr;

    // UI panels
    ViewportWidget* m_viewport = nullptr;
    GameViewWidget* m_game_view = nullptr;
    HierarchyPanel* m_hierarchy = nullptr;
    InspectorPanel* m_inspector = nullptr;
    AssetBrowser* m_assets = nullptr;
    ConsolePanel* m_console = nullptr;

    // Toolbar actions - Transform modes
    QAction* m_select_action = nullptr;
    QAction* m_translate_action = nullptr;
    QAction* m_rotate_action = nullptr;
    QAction* m_scale_action = nullptr;

    // Toolbar actions - Play controls
    QAction* m_play_action = nullptr;
    QAction* m_pause_action = nullptr;
    QAction* m_stop_action = nullptr;

    QString m_current_project_path;
    bool m_engine_initialized = false;
};

} // namespace editor

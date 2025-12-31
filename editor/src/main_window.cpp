#include "main_window.hpp"
#include "viewport_widget.hpp"
#include "hierarchy_panel.hpp"
#include "inspector_panel.hpp"
#include "asset_browser.hpp"
#include "console_panel.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/scene/components.hpp>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QApplication>
#include <QDir>

namespace editor {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Create state first
    m_state = std::make_unique<EditorState>(this);
    m_world = std::make_unique<engine::scene::World>();

    m_state->set_world(m_world.get());

    setup_ui();
    setup_menus();
    setup_toolbar();
    setup_panels();
    setup_connections();

    restore_layout();

    // Log welcome message
    if (m_console) {
        m_console->log(engine::core::LogLevel::Info, "Engine Editor started", "Editor");
    }
}

MainWindow::~MainWindow() {
    shutdown_engine();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    init_engine();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    save_layout();
    shutdown_engine();
    event->accept();
}

void MainWindow::setup_ui() {
    setWindowTitle("Engine Editor");
    resize(1600, 900);
    setDockNestingEnabled(true);

    statusBar()->showMessage("Ready");
}

void MainWindow::setup_menus() {
    // File menu
    auto* file_menu = menuBar()->addMenu("&File");

    auto* new_action = file_menu->addAction("&New Project");
    new_action->setShortcut(QKeySequence::New);
    connect(new_action, &QAction::triggered, this, &MainWindow::on_new_project);

    auto* open_action = file_menu->addAction("&Open Project...");
    open_action->setShortcut(QKeySequence::Open);
    connect(open_action, &QAction::triggered, this, &MainWindow::on_open_project);

    file_menu->addSeparator();

    auto* save_action = file_menu->addAction("&Save");
    save_action->setShortcut(QKeySequence::Save);
    connect(save_action, &QAction::triggered, this, &MainWindow::on_save_project);

    auto* save_as_action = file_menu->addAction("Save &As...");
    save_as_action->setShortcut(QKeySequence::SaveAs);
    connect(save_as_action, &QAction::triggered, this, &MainWindow::on_save_project_as);

    file_menu->addSeparator();

    auto* exit_action = file_menu->addAction("E&xit");
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QMainWindow::close);

    // Edit menu
    auto* edit_menu = menuBar()->addMenu("&Edit");

    auto* undo_action = edit_menu->addAction("&Undo");
    undo_action->setShortcut(QKeySequence::Undo);
    connect(undo_action, &QAction::triggered, this, &MainWindow::on_undo);

    auto* redo_action = edit_menu->addAction("&Redo");
    redo_action->setShortcut(QKeySequence::Redo);
    connect(redo_action, &QAction::triggered, this, &MainWindow::on_redo);

    edit_menu->addSeparator();

    auto* duplicate_action = edit_menu->addAction("&Duplicate");
    duplicate_action->setShortcut(QKeySequence("Ctrl+D"));
    connect(duplicate_action, &QAction::triggered, this, &MainWindow::on_duplicate);

    auto* delete_action = edit_menu->addAction("De&lete");
    delete_action->setShortcut(QKeySequence::Delete);
    connect(delete_action, &QAction::triggered, this, &MainWindow::on_delete);

    // Create menu
    auto* create_menu = menuBar()->addMenu("&Create");

    auto* empty_action = create_menu->addAction("&Empty Entity");
    connect(empty_action, &QAction::triggered, this, &MainWindow::on_create_empty_entity);

    create_menu->addSeparator();

    auto* primitives_menu = create_menu->addMenu("&Primitives");
    auto* cube_action = primitives_menu->addAction("&Cube");
    connect(cube_action, &QAction::triggered, this, &MainWindow::on_create_cube);
    auto* sphere_action = primitives_menu->addAction("&Sphere");
    connect(sphere_action, &QAction::triggered, this, &MainWindow::on_create_sphere);

    create_menu->addSeparator();

    auto* camera_action = create_menu->addAction("&Camera");
    connect(camera_action, &QAction::triggered, this, &MainWindow::on_create_camera);

    auto* lights_menu = create_menu->addMenu("&Light");
    auto* dir_light_action = lights_menu->addAction("&Directional Light");
    connect(dir_light_action, &QAction::triggered, this, &MainWindow::on_create_directional_light);
    auto* point_light_action = lights_menu->addAction("&Point Light");
    connect(point_light_action, &QAction::triggered, this, &MainWindow::on_create_point_light);

    // View menu
    auto* view_menu = menuBar()->addMenu("&View");

    auto* hierarchy_action = view_menu->addAction("&Hierarchy");
    hierarchy_action->setCheckable(true);
    hierarchy_action->setChecked(true);
    connect(hierarchy_action, &QAction::triggered, this, &MainWindow::on_toggle_hierarchy);

    auto* inspector_action = view_menu->addAction("&Inspector");
    inspector_action->setCheckable(true);
    inspector_action->setChecked(true);
    connect(inspector_action, &QAction::triggered, this, &MainWindow::on_toggle_inspector);

    auto* assets_action = view_menu->addAction("&Assets");
    assets_action->setCheckable(true);
    assets_action->setChecked(true);
    connect(assets_action, &QAction::triggered, this, &MainWindow::on_toggle_assets);

    auto* console_action = view_menu->addAction("&Console");
    console_action->setCheckable(true);
    console_action->setChecked(true);
    connect(console_action, &QAction::triggered, this, &MainWindow::on_toggle_console);

    view_menu->addSeparator();

    auto* reset_layout_action = view_menu->addAction("&Reset Layout");
    connect(reset_layout_action, &QAction::triggered, this, &MainWindow::on_reset_layout);

    // Help menu
    auto* help_menu = menuBar()->addMenu("&Help");
    help_menu->addAction("&About");
}

void MainWindow::setup_toolbar() {
    auto* toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);

    // Transform mode buttons
    auto* select_action = toolbar->addAction("Select");
    select_action->setCheckable(true);
    select_action->setChecked(true);
    connect(select_action, &QAction::triggered, [this]() {
        m_state->set_mode(EditorState::Mode::Select);
    });

    auto* translate_action = toolbar->addAction("Move");
    translate_action->setCheckable(true);
    translate_action->setShortcut(QKeySequence("W"));
    connect(translate_action, &QAction::triggered, [this]() {
        m_state->set_mode(EditorState::Mode::Translate);
    });

    auto* rotate_action = toolbar->addAction("Rotate");
    rotate_action->setCheckable(true);
    rotate_action->setShortcut(QKeySequence("E"));
    connect(rotate_action, &QAction::triggered, [this]() {
        m_state->set_mode(EditorState::Mode::Rotate);
    });

    auto* scale_action = toolbar->addAction("Scale");
    scale_action->setCheckable(true);
    scale_action->setShortcut(QKeySequence("R"));
    connect(scale_action, &QAction::triggered, [this]() {
        m_state->set_mode(EditorState::Mode::Scale);
    });

    // Mode action group
    auto* mode_group = new QActionGroup(this);
    mode_group->addAction(select_action);
    mode_group->addAction(translate_action);
    mode_group->addAction(rotate_action);
    mode_group->addAction(scale_action);

    toolbar->addSeparator();

    // Play controls
    m_play_action = toolbar->addAction("Play");
    m_play_action->setShortcut(QKeySequence("Ctrl+P"));
    connect(m_play_action, &QAction::triggered, this, &MainWindow::on_play);

    m_pause_action = toolbar->addAction("Pause");
    m_pause_action->setEnabled(false);
    connect(m_pause_action, &QAction::triggered, this, &MainWindow::on_pause);

    m_stop_action = toolbar->addAction("Stop");
    m_stop_action->setEnabled(false);
    connect(m_stop_action, &QAction::triggered, this, &MainWindow::on_stop);
}

void MainWindow::setup_panels() {
    // Create viewport (central widget)
    m_viewport = new ViewportWidget(m_state.get(), this);
    setCentralWidget(m_viewport);

    // Create hierarchy panel (left)
    m_hierarchy = new HierarchyPanel(m_state.get(), this);
    addDockWidget(Qt::LeftDockWidgetArea, m_hierarchy);

    // Create inspector panel (right)
    m_inspector = new InspectorPanel(m_state.get(), this);
    addDockWidget(Qt::RightDockWidgetArea, m_inspector);

    // Create asset browser (bottom)
    m_assets = new AssetBrowser(m_state.get(), this);
    addDockWidget(Qt::BottomDockWidgetArea, m_assets);
    m_assets->set_root_path(QDir::currentPath() + "/assets");

    // Create console panel (bottom, tabbed with assets)
    m_console = new ConsolePanel(m_state.get(), this);
    tabifyDockWidget(m_assets, m_console);

    // Set default sizes
    m_hierarchy->setMinimumWidth(200);
    m_inspector->setMinimumWidth(220);
}

void MainWindow::setup_connections() {
    connect(m_viewport, &ViewportWidget::entity_picked,
            this, &MainWindow::on_entity_picked);
    connect(m_viewport, &ViewportWidget::viewport_resized,
            this, &MainWindow::on_viewport_resized);

    connect(m_hierarchy, &HierarchyPanel::entity_double_clicked,
            [this](engine::scene::Entity entity) {
                m_state->select(entity);
                m_viewport->focus_selection();
            });
}

void MainWindow::init_engine() {
    if (m_engine_initialized) return;

    // Create renderer
    m_renderer = engine::render::create_bgfx_renderer();
    if (m_renderer) {
        const float dpr = m_viewport->devicePixelRatioF();
        const uint32_t w = static_cast<uint32_t>(m_viewport->width() * dpr);
        const uint32_t h = static_cast<uint32_t>(m_viewport->height() * dpr);

        if (m_renderer->init(m_viewport->native_handle(), w, h)) {
            m_state->set_renderer(m_renderer.get());
            m_engine_initialized = true;

            if (m_console) {
                m_console->log(engine::core::LogLevel::Info,
                              "Renderer initialized", "Engine");
            }

            create_demo_scene();
        } else {
            if (m_console) {
                m_console->log(engine::core::LogLevel::Error,
                              "Failed to initialize renderer", "Engine");
            }
        }
    }
}

void MainWindow::shutdown_engine() {
    if (!m_engine_initialized) return;

    if (m_renderer) {
        m_renderer->shutdown();
        m_renderer.reset();
    }

    m_engine_initialized = false;
}

void MainWindow::create_demo_scene() {
    if (!m_world || !m_renderer) return;

    // Create a cube entity
    auto cube = m_world->create("Cube");
    m_world->emplace<engine::scene::LocalTransform>(cube, engine::core::Vec3{0.0f, 0.0f, 0.0f});
    m_world->emplace<engine::scene::WorldTransform>(cube);

    auto cube_mesh = m_renderer->create_primitive(engine::render::PrimitiveMesh::Cube, 1.0f);
    engine::scene::MeshRenderer mesh_renderer;
    mesh_renderer.mesh = engine::scene::MeshHandle{cube_mesh.id};
    m_world->emplace<engine::scene::MeshRenderer>(cube, mesh_renderer);

    // Create a camera
    auto camera = m_world->create("Main Camera");
    m_world->emplace<engine::scene::LocalTransform>(camera, engine::core::Vec3{0.0f, 2.0f, 5.0f});
    m_world->emplace<engine::scene::WorldTransform>(camera);
    m_world->emplace<engine::scene::Camera>(camera);

    // Create a directional light
    auto light = m_world->create("Directional Light");
    engine::scene::LocalTransform light_tf{engine::core::Vec3{0.0f, 10.0f, 0.0f}};
    light_tf.set_euler(engine::core::Vec3{glm::radians(-45.0f), 0.0f, 0.0f});
    m_world->emplace<engine::scene::LocalTransform>(light, light_tf);
    m_world->emplace<engine::scene::WorldTransform>(light);
    engine::scene::Light dir_light;
    dir_light.type = engine::scene::LightType::Directional;
    m_world->emplace<engine::scene::Light>(light, dir_light);

    // Refresh hierarchy
    m_hierarchy->refresh();

    if (m_console) {
        m_console->log(engine::core::LogLevel::Info, "Demo scene created", "Editor");
    }
}

void MainWindow::on_new_project() {
    m_world->clear();
    m_state->clear_selection();
    m_current_project_path.clear();
    m_hierarchy->refresh();
    setWindowTitle("Engine Editor - Untitled");
}

void MainWindow::on_open_project() {
    QString path = QFileDialog::getOpenFileName(this, "Open Project",
                                                 QString(), "Project Files (*.project)");
    if (path.isEmpty()) return;

    // TODO: Load project
    m_current_project_path = path;
    setWindowTitle("Engine Editor - " + QFileInfo(path).baseName());
}

void MainWindow::on_save_project() {
    if (m_current_project_path.isEmpty()) {
        on_save_project_as();
        return;
    }
    // TODO: Save project
}

void MainWindow::on_save_project_as() {
    QString path = QFileDialog::getSaveFileName(this, "Save Project",
                                                 QString(), "Project Files (*.project)");
    if (path.isEmpty()) return;

    m_current_project_path = path;
    // TODO: Save project
    setWindowTitle("Engine Editor - " + QFileInfo(path).baseName());
}

void MainWindow::on_undo() {
    m_state->undo_stack()->undo();
    m_hierarchy->refresh();
    m_inspector->refresh();
}

void MainWindow::on_redo() {
    m_state->undo_stack()->redo();
    m_hierarchy->refresh();
    m_inspector->refresh();
}

void MainWindow::on_duplicate() {
    // TODO: Duplicate selected entities
}

void MainWindow::on_delete() {
    for (auto entity : m_state->selection()) {
        auto* cmd = new DeleteEntityCommand(m_state.get(), entity);
        m_state->undo_stack()->push(cmd);
    }
    m_hierarchy->refresh();
}

void MainWindow::on_create_empty_entity() {
    auto* cmd = new CreateEntityCommand(m_state.get(), "Entity");
    m_state->undo_stack()->push(cmd);
    m_hierarchy->refresh();
}

void MainWindow::on_create_cube() {
    if (!m_renderer) return;

    auto* cmd = new CreateEntityCommand(m_state.get(), "Cube");
    m_state->undo_stack()->push(cmd);

    auto entity = cmd->created_entity();
    if (entity != engine::scene::NullEntity) {
        auto mesh = m_renderer->create_primitive(engine::render::PrimitiveMesh::Cube, 1.0f);
        engine::scene::MeshRenderer renderer;
        renderer.mesh = engine::scene::MeshHandle{mesh.id};
        m_world->emplace<engine::scene::MeshRenderer>(entity, renderer);
    }
    m_hierarchy->refresh();
}

void MainWindow::on_create_sphere() {
    if (!m_renderer) return;

    auto* cmd = new CreateEntityCommand(m_state.get(), "Sphere");
    m_state->undo_stack()->push(cmd);

    auto entity = cmd->created_entity();
    if (entity != engine::scene::NullEntity) {
        auto mesh = m_renderer->create_primitive(engine::render::PrimitiveMesh::Sphere, 1.0f);
        engine::scene::MeshRenderer renderer;
        renderer.mesh = engine::scene::MeshHandle{mesh.id};
        m_world->emplace<engine::scene::MeshRenderer>(entity, renderer);
    }
    m_hierarchy->refresh();
}

void MainWindow::on_create_camera() {
    auto* cmd = new CreateEntityCommand(m_state.get(), "Camera");
    m_state->undo_stack()->push(cmd);

    auto entity = cmd->created_entity();
    if (entity != engine::scene::NullEntity) {
        m_world->emplace<engine::scene::Camera>(entity);
    }
    m_hierarchy->refresh();
}

void MainWindow::on_create_directional_light() {
    auto* cmd = new CreateEntityCommand(m_state.get(), "Directional Light");
    m_state->undo_stack()->push(cmd);

    auto entity = cmd->created_entity();
    if (entity != engine::scene::NullEntity) {
        engine::scene::Light light;
        light.type = engine::scene::LightType::Directional;
        m_world->emplace<engine::scene::Light>(entity, light);
    }
    m_hierarchy->refresh();
}

void MainWindow::on_create_point_light() {
    auto* cmd = new CreateEntityCommand(m_state.get(), "Point Light");
    m_state->undo_stack()->push(cmd);

    auto entity = cmd->created_entity();
    if (entity != engine::scene::NullEntity) {
        engine::scene::Light light;
        light.type = engine::scene::LightType::Point;
        m_world->emplace<engine::scene::Light>(entity, light);
    }
    m_hierarchy->refresh();
}

void MainWindow::on_toggle_hierarchy() {
    m_hierarchy->setVisible(!m_hierarchy->isVisible());
}

void MainWindow::on_toggle_inspector() {
    m_inspector->setVisible(!m_inspector->isVisible());
}

void MainWindow::on_toggle_assets() {
    m_assets->setVisible(!m_assets->isVisible());
}

void MainWindow::on_toggle_console() {
    m_console->setVisible(!m_console->isVisible());
}

void MainWindow::on_reset_layout() {
    // Reset dock positions
    removeDockWidget(m_hierarchy);
    removeDockWidget(m_inspector);
    removeDockWidget(m_assets);
    removeDockWidget(m_console);

    addDockWidget(Qt::LeftDockWidgetArea, m_hierarchy);
    addDockWidget(Qt::RightDockWidgetArea, m_inspector);
    addDockWidget(Qt::BottomDockWidgetArea, m_assets);
    tabifyDockWidget(m_assets, m_console);

    m_hierarchy->show();
    m_inspector->show();
    m_assets->show();
    m_console->show();
}

void MainWindow::on_play() {
    m_state->set_playing(true);
    m_play_action->setEnabled(false);
    m_pause_action->setEnabled(true);
    m_stop_action->setEnabled(true);

    if (m_console) {
        m_console->log(engine::core::LogLevel::Info, "Play mode started", "Editor");
    }
}

void MainWindow::on_pause() {
    // TODO: Toggle pause
    m_play_action->setEnabled(true);
    m_pause_action->setEnabled(false);
}

void MainWindow::on_stop() {
    m_state->set_playing(false);
    m_play_action->setEnabled(true);
    m_pause_action->setEnabled(false);
    m_stop_action->setEnabled(false);

    if (m_console) {
        m_console->log(engine::core::LogLevel::Info, "Play mode stopped", "Editor");
    }
}

void MainWindow::on_entity_picked(engine::scene::Entity entity) {
    if (entity != engine::scene::NullEntity) {
        statusBar()->showMessage(QString("Selected entity %1")
            .arg(static_cast<uint32_t>(entity)));
    } else {
        statusBar()->showMessage("Ready");
    }
}

void MainWindow::on_viewport_resized(int width, int height) {
    if (m_renderer && m_engine_initialized) {
        const float dpr = m_viewport->devicePixelRatioF();
        const uint32_t w = static_cast<uint32_t>(width * dpr);
        const uint32_t h = static_cast<uint32_t>(height * dpr);
        m_renderer->resize(w, h);
    }
}

void MainWindow::save_layout() {
    QSettings settings("Engine", "Editor");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::restore_layout() {
    QSettings settings("Engine", "Editor");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

} // namespace editor

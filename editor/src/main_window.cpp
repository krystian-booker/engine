#include "main_window.hpp"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setup_ui();
    setup_menus();
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    setWindowTitle("Engine Editor");
    resize(1280, 720);
    statusBar()->showMessage("Ready");
}

void MainWindow::setup_menus() {
    auto* file_menu = menuBar()->addMenu("&File");

    auto* new_action = file_menu->addAction("&New Project");
    new_action->setShortcut(QKeySequence::New);

    auto* open_action = file_menu->addAction("&Open Project...");
    open_action->setShortcut(QKeySequence::Open);

    file_menu->addSeparator();

    auto* exit_action = file_menu->addAction("E&xit");
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QMainWindow::close);

    auto* help_menu = menuBar()->addMenu("&Help");
    help_menu->addAction("&About");
}

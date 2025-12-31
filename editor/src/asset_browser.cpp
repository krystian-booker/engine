#include "asset_browser.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>

namespace editor {

AssetBrowser::AssetBrowser(EditorState* state, QWidget* parent)
    : QDockWidget("Assets", parent)
    , m_state(state)
{
    setup_ui();
    setup_connections();
}

AssetBrowser::~AssetBrowser() = default;

void AssetBrowser::setup_ui() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    auto* toolbar = new QToolBar(container);
    toolbar->setIconSize(QSize(16, 16));

    auto* refresh_action = toolbar->addAction("Refresh");
    connect(refresh_action, &QAction::triggered, this, &AssetBrowser::refresh);

    auto* import_action = toolbar->addAction("Import");
    connect(import_action, &QAction::triggered, this, &AssetBrowser::import_asset);

    auto* new_folder_action = toolbar->addAction("New Folder");
    connect(new_folder_action, &QAction::triggered, this, &AssetBrowser::create_folder);

    layout->addWidget(toolbar);

    // Splitter with folder tree and file list
    m_splitter = new QSplitter(Qt::Horizontal, container);

    // Folder tree view
    m_folder_tree = new QTreeView(m_splitter);
    m_folder_model = new QFileSystemModel(this);
    m_folder_model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    m_folder_tree->setModel(m_folder_model);
    m_folder_tree->setHeaderHidden(true);
    // Hide all columns except name
    for (int i = 1; i < m_folder_model->columnCount(); ++i) {
        m_folder_tree->hideColumn(i);
    }

    // File list view
    m_file_list = new QListView(m_splitter);
    m_file_model = new QFileSystemModel(this);
    m_file_model->setFilter(QDir::Files | QDir::NoDotAndDotDot);
    m_file_list->setModel(m_file_model);
    m_file_list->setViewMode(QListView::IconMode);
    m_file_list->setIconSize(QSize(64, 64));
    m_file_list->setGridSize(QSize(80, 100));
    m_file_list->setResizeMode(QListView::Adjust);
    m_file_list->setContextMenuPolicy(Qt::CustomContextMenu);

    m_splitter->addWidget(m_folder_tree);
    m_splitter->addWidget(m_file_list);
    m_splitter->setSizes({200, 400});

    layout->addWidget(m_splitter);
    setWidget(container);

    // Context menu
    m_context_menu = new QMenu(this);
    m_context_menu->addAction("Import", this, &AssetBrowser::import_asset);
    m_context_menu->addAction("New Folder", this, &AssetBrowser::create_folder);
    m_context_menu->addSeparator();
    m_context_menu->addAction("Rename", this, &AssetBrowser::rename_selected, QKeySequence("F2"));
    m_context_menu->addAction("Delete", this, &AssetBrowser::delete_selected, QKeySequence::Delete);
    m_context_menu->addSeparator();
    m_context_menu->addAction("Show in Explorer", this, &AssetBrowser::show_in_explorer);
    m_context_menu->addAction("Refresh", this, &AssetBrowser::refresh);
}

void AssetBrowser::setup_connections() {
    connect(m_folder_tree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &AssetBrowser::on_folder_selected);

    connect(m_file_list->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &AssetBrowser::on_asset_selected);

    connect(m_file_list, &QListView::doubleClicked,
            this, &AssetBrowser::on_asset_double_clicked);

    connect(m_file_list, &QListView::customContextMenuRequested,
            this, &AssetBrowser::on_context_menu);
}

void AssetBrowser::set_root_path(const QString& path) {
    m_root_path = path;

    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    m_folder_model->setRootPath(path);
    m_folder_tree->setRootIndex(m_folder_model->index(path));

    update_file_view(path);
}

QString AssetBrowser::current_path() const {
    auto index = m_folder_tree->currentIndex();
    if (index.isValid()) {
        return m_folder_model->filePath(index);
    }
    return m_root_path;
}

void AssetBrowser::on_folder_selected(const QModelIndex& index) {
    if (!index.isValid()) return;

    QString path = m_folder_model->filePath(index);
    update_file_view(path);
}

void AssetBrowser::on_asset_selected(const QModelIndex& index) {
    if (!index.isValid()) return;

    QString path = m_file_model->filePath(index);
    emit asset_selected(path);
}

void AssetBrowser::on_asset_double_clicked(const QModelIndex& index) {
    if (!index.isValid()) return;

    QString path = m_file_model->filePath(index);
    emit asset_double_clicked(path);
}

void AssetBrowser::on_context_menu(const QPoint& pos) {
    m_context_menu->exec(m_file_list->mapToGlobal(pos));
}

void AssetBrowser::update_file_view(const QString& path) {
    m_file_model->setRootPath(path);
    m_file_list->setRootIndex(m_file_model->index(path));
}

void AssetBrowser::import_asset() {
    // TODO: Show file dialog and import asset
    emit asset_import_requested(current_path());
}

void AssetBrowser::create_folder() {
    QString name = QInputDialog::getText(this, "New Folder", "Folder name:");
    if (name.isEmpty()) return;

    QDir dir(current_path());
    if (!dir.mkdir(name)) {
        QMessageBox::warning(this, "Error", "Failed to create folder.");
    }
}

void AssetBrowser::rename_selected() {
    auto index = m_file_list->currentIndex();
    if (!index.isValid()) return;

    QString old_path = m_file_model->filePath(index);
    QFileInfo info(old_path);

    QString new_name = QInputDialog::getText(this, "Rename",
                                              "New name:",
                                              QLineEdit::Normal,
                                              info.fileName());
    if (new_name.isEmpty() || new_name == info.fileName()) return;

    QString new_path = info.dir().filePath(new_name);
    if (!QFile::rename(old_path, new_path)) {
        QMessageBox::warning(this, "Error", "Failed to rename file.");
    }
}

void AssetBrowser::delete_selected() {
    auto index = m_file_list->currentIndex();
    if (!index.isValid()) return;

    QString path = m_file_model->filePath(index);
    QFileInfo info(path);

    auto result = QMessageBox::question(this, "Delete",
                                         QString("Delete '%1'?").arg(info.fileName()),
                                         QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        if (info.isDir()) {
            QDir(path).removeRecursively();
        } else {
            QFile::remove(path);
        }
    }
}

void AssetBrowser::show_in_explorer() {
    QString path = current_path();
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void AssetBrowser::refresh() {
    // Force refresh by resetting the model
    QString path = current_path();
    m_folder_model->setRootPath("");
    m_folder_model->setRootPath(m_root_path);
    m_folder_tree->setRootIndex(m_folder_model->index(m_root_path));
    update_file_view(path);
}

QIcon AssetBrowser::icon_for_file(const QString& path) const {
    // TODO: Return custom icons based on file type
    Q_UNUSED(path)
    return QIcon();
}

QString AssetBrowser::asset_type(const QString& path) const {
    QFileInfo info(path);
    QString ext = info.suffix().toLower();

    if (ext == "gltf" || ext == "glb" || ext == "fbx" || ext == "obj") {
        return "Model";
    } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "bmp") {
        return "Texture";
    } else if (ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac") {
        return "Audio";
    } else if (ext == "vs" || ext == "fs" || ext == "glsl" || ext == "hlsl") {
        return "Shader";
    } else if (ext == "json") {
        return "Data";
    } else if (ext == "scene") {
        return "Scene";
    } else if (ext == "prefab") {
        return "Prefab";
    }

    return "Unknown";
}

} // namespace editor

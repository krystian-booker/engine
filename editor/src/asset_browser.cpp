#include "asset_browser.hpp"
#include "breadcrumb_bar.hpp"
#include "asset_item_model.hpp"
#include "asset_item_delegate.hpp"
#include "thumbnail_cache.hpp"
#include "thumbnail_generator.hpp"
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
#include <QFileDialog>
#include <QSortFilterProxyModel>
#include <QKeyEvent>
#include <QApplication>

namespace editor {

AssetBrowser::AssetBrowser(EditorState* state, QWidget* parent)
    : QDockWidget("Assets", parent)
    , m_state(state)
{
    // Initialize thumbnail system
    m_thumbnail_cache = new ThumbnailCache(this);
    m_thumbnail_cache->set_cache_dir(QDir::tempPath() + "/engine_editor_thumbnails");

    m_thumbnail_generator = new ThumbnailGenerator(this);
    m_thumbnail_generator->set_cache(m_thumbnail_cache);

    // Initialize custom model and delegate
    m_asset_model = new AssetItemModel(this);
    m_item_delegate = new AssetItemDelegate(this);

    // Connect thumbnail generator to model
    connect(m_thumbnail_generator, &ThumbnailGenerator::thumbnail_ready,
            this, [this](const QString& path, const QIcon& icon) {
        m_asset_model->set_thumbnail(path, icon);
    });

    connect(m_asset_model, &AssetItemModel::thumbnail_needed,
            this, [this](const QString& path, AssetType type) {
        m_thumbnail_generator->request(path, type, m_icon_size);
    });

    // Load favorites from settings
    QSettings settings;
    QStringList favorites = settings.value("AssetBrowser/Favorites").toStringList();
    m_favorites = QSet<QString>(favorites.begin(), favorites.end());

    setup_ui();
    setup_connections();
}

AssetBrowser::~AssetBrowser() {
    // Save favorites to settings
    QSettings settings;
    settings.setValue("AssetBrowser/Favorites", QStringList(m_favorites.begin(), m_favorites.end()));
}

void AssetBrowser::setup_ui() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setup_toolbar();
    layout->addWidget(m_breadcrumb_bar);

    // Main toolbar
    auto* toolbar = new QWidget(container);
    auto* toolbar_layout = new QHBoxLayout(toolbar);
    toolbar_layout->setContentsMargins(4, 4, 4, 4);
    toolbar_layout->setSpacing(6);

    // Search box
    m_search_box = new QLineEdit(toolbar);
    m_search_box->setPlaceholderText("Search...");
    m_search_box->setClearButtonEnabled(true);
    m_search_box->setMaximumWidth(200);
    toolbar_layout->addWidget(m_search_box);

    // Type filter
    m_filter_combo = new QComboBox(toolbar);
    m_filter_combo->addItem("All", static_cast<int>(AssetTypeFilter::All));
    m_filter_combo->addItem("Textures", static_cast<int>(AssetTypeFilter::Textures));
    m_filter_combo->addItem("Meshes", static_cast<int>(AssetTypeFilter::Meshes));
    m_filter_combo->addItem("Materials", static_cast<int>(AssetTypeFilter::Materials));
    m_filter_combo->addItem("Audio", static_cast<int>(AssetTypeFilter::Audio));
    m_filter_combo->addItem("Shaders", static_cast<int>(AssetTypeFilter::Shaders));
    m_filter_combo->addItem("Scenes", static_cast<int>(AssetTypeFilter::Scenes));
    m_filter_combo->addItem("Prefabs", static_cast<int>(AssetTypeFilter::Prefabs));
    m_filter_combo->setMaximumWidth(100);
    toolbar_layout->addWidget(m_filter_combo);

    toolbar_layout->addStretch();

    // Create button with menu
    m_create_btn = new QToolButton(toolbar);
    m_create_btn->setText("Create");
    m_create_btn->setPopupMode(QToolButton::InstantPopup);
    m_create_menu = new QMenu(m_create_btn);
    m_create_menu->addAction("Folder", this, &AssetBrowser::create_folder);
    m_create_menu->addSeparator();
    m_create_menu->addAction("Material", this, &AssetBrowser::create_material);
    m_create_menu->addAction("Scene", this, &AssetBrowser::create_scene);
    m_create_menu->addAction("Prefab", this, &AssetBrowser::create_prefab);
    m_create_btn->setMenu(m_create_menu);
    toolbar_layout->addWidget(m_create_btn);

    // Import button
    auto* import_btn = new QToolButton(toolbar);
    import_btn->setText("Import");
    connect(import_btn, &QToolButton::clicked, this, &AssetBrowser::import_asset);
    toolbar_layout->addWidget(import_btn);

    toolbar_layout->addSpacing(12);

    // Icon size slider
    m_size_label = new QLabel("Size:", toolbar);
    toolbar_layout->addWidget(m_size_label);

    m_size_slider = new QSlider(Qt::Horizontal, toolbar);
    m_size_slider->setRange(32, 128);
    m_size_slider->setValue(m_icon_size);
    m_size_slider->setMaximumWidth(80);
    m_size_slider->setToolTip("Thumbnail size");
    toolbar_layout->addWidget(m_size_slider);

    // View mode toggle
    m_view_mode_btn = new QToolButton(toolbar);
    m_view_mode_btn->setText("Grid");
    m_view_mode_btn->setCheckable(true);
    m_view_mode_btn->setToolTip("Toggle list/grid view");
    toolbar_layout->addWidget(m_view_mode_btn);

    // Refresh button
    auto* refresh_btn = new QToolButton(toolbar);
    refresh_btn->setText("Refresh");
    connect(refresh_btn, &QToolButton::clicked, this, &AssetBrowser::refresh);
    toolbar_layout->addWidget(refresh_btn);

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
    m_file_list->setIconSize(QSize(m_icon_size, m_icon_size));
    m_file_list->setGridSize(QSize(m_icon_size + 20, m_icon_size + 36));
    m_file_list->setResizeMode(QListView::Adjust);
    m_file_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_file_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_file_list->setDragEnabled(true);
    m_file_list->setWordWrap(true);

    // Install event filter for keyboard navigation
    m_file_list->installEventFilter(this);
    m_file_list->setFocusPolicy(Qt::StrongFocus);

    m_splitter->addWidget(m_folder_tree);
    m_splitter->addWidget(m_file_list);
    m_splitter->setSizes({200, 400});

    layout->addWidget(m_splitter);
    setWidget(container);

    // Context menu
    m_context_menu = new QMenu(this);
    m_context_menu->addAction("Import", this, &AssetBrowser::import_asset);
    m_context_menu->addMenu(m_create_menu);
    m_context_menu->addSeparator();
    m_context_menu->addAction("Rename", this, &AssetBrowser::rename_selected, QKeySequence("F2"));
    m_context_menu->addAction("Delete", this, &AssetBrowser::delete_selected, QKeySequence::Delete);
    m_context_menu->addSeparator();
    m_context_menu->addAction("Show in Explorer", this, &AssetBrowser::show_in_explorer);
    m_context_menu->addAction("Refresh", this, &AssetBrowser::refresh);

    // Apply styling
    setStyleSheet(R"(
        QToolButton {
            padding: 4px 8px;
        }
        QComboBox {
            padding: 2px 4px;
        }
    )");
}

void AssetBrowser::setup_toolbar() {
    m_breadcrumb_bar = new BreadcrumbBar(this);
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

    // Toolbar connections
    connect(m_search_box, &QLineEdit::textChanged,
            this, &AssetBrowser::on_search_changed);

    connect(m_filter_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AssetBrowser::on_filter_changed);

    connect(m_size_slider, &QSlider::valueChanged,
            this, &AssetBrowser::on_icon_size_changed);

    connect(m_view_mode_btn, &QToolButton::toggled,
            this, &AssetBrowser::on_view_mode_toggled);

    connect(m_breadcrumb_bar, &BreadcrumbBar::path_clicked,
            this, &AssetBrowser::on_breadcrumb_clicked);
}

void AssetBrowser::set_root_path(const QString& path) {
    m_root_path = path;

    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    m_folder_model->setRootPath(path);
    m_folder_tree->setRootIndex(m_folder_model->index(path));

    m_breadcrumb_bar->set_root_path(path);
    m_breadcrumb_bar->set_path(path);

    update_file_view(path);
}

QString AssetBrowser::current_path() const {
    auto index = m_folder_tree->currentIndex();
    if (index.isValid()) {
        return m_folder_model->filePath(index);
    }
    return m_root_path;
}

void AssetBrowser::set_icon_size(int size) {
    m_icon_size = qBound(32, size, 128);
    m_file_list->setIconSize(QSize(m_icon_size, m_icon_size));
    m_file_list->setGridSize(QSize(m_icon_size + 20, m_icon_size + 36));
    m_size_slider->setValue(m_icon_size);
}

void AssetBrowser::on_folder_selected(const QModelIndex& index) {
    if (!index.isValid()) return;

    QString path = m_folder_model->filePath(index);
    m_breadcrumb_bar->set_path(path);
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

void AssetBrowser::on_breadcrumb_clicked(const QString& path) {
    navigate_to(path);
}

void AssetBrowser::on_search_changed(const QString& text) {
    m_search_text = text;
    apply_filters();
}

void AssetBrowser::on_filter_changed(int index) {
    m_type_filter = static_cast<AssetTypeFilter>(m_filter_combo->itemData(index).toInt());
    apply_filters();
}

void AssetBrowser::on_icon_size_changed(int value) {
    m_icon_size = value;
    m_file_list->setIconSize(QSize(m_icon_size, m_icon_size));
    m_file_list->setGridSize(QSize(m_icon_size + 20, m_icon_size + 36));
}

void AssetBrowser::on_view_mode_toggled() {
    m_list_mode = m_view_mode_btn->isChecked();
    if (m_list_mode) {
        m_file_list->setViewMode(QListView::ListMode);
        m_view_mode_btn->setText("List");
    } else {
        m_file_list->setViewMode(QListView::IconMode);
        m_view_mode_btn->setText("Grid");
    }
}

void AssetBrowser::navigate_to(const QString& path) {
    QModelIndex index = m_folder_model->index(path);
    if (index.isValid()) {
        m_folder_tree->setCurrentIndex(index);
        m_folder_tree->expand(index);
        m_breadcrumb_bar->set_path(path);
        update_file_view(path);
    }
}

void AssetBrowser::update_file_view(const QString& path) {
    m_file_model->setRootPath(path);
    m_file_list->setRootIndex(m_file_model->index(path));
    apply_filters();
}

void AssetBrowser::apply_filters() {
    QStringList name_filters;

    // Build extension filter based on type
    switch (m_type_filter) {
        case AssetTypeFilter::Textures:
            name_filters << "*.png" << "*.jpg" << "*.jpeg" << "*.tga" << "*.bmp";
            break;
        case AssetTypeFilter::Meshes:
            name_filters << "*.gltf" << "*.glb" << "*.fbx" << "*.obj";
            break;
        case AssetTypeFilter::Materials:
            name_filters << "*.mat" << "*.material";
            break;
        case AssetTypeFilter::Audio:
            name_filters << "*.wav" << "*.mp3" << "*.ogg" << "*.flac";
            break;
        case AssetTypeFilter::Shaders:
            name_filters << "*.vs" << "*.fs" << "*.glsl" << "*.hlsl" << "*.shader";
            break;
        case AssetTypeFilter::Scenes:
            name_filters << "*.scene";
            break;
        case AssetTypeFilter::Prefabs:
            name_filters << "*.prefab";
            break;
        case AssetTypeFilter::All:
        default:
            name_filters << "*";
            break;
    }

    // Add search text filter if present
    if (!m_search_text.isEmpty()) {
        for (auto& filter : name_filters) {
            // Convert *.ext to *search*.ext
            if (filter.startsWith("*.")) {
                QString ext = filter.mid(1); // .ext
                filter = "*" + m_search_text + "*" + ext;
            } else if (filter == "*") {
                filter = "*" + m_search_text + "*";
            }
        }
    }

    m_file_model->setNameFilters(name_filters);
    m_file_model->setNameFilterDisables(false);
}

bool AssetBrowser::matches_filter(const QString& path) const {
    QFileInfo info(path);
    QString ext = info.suffix().toLower();

    switch (m_type_filter) {
        case AssetTypeFilter::Textures:
            return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "bmp";
        case AssetTypeFilter::Meshes:
            return ext == "gltf" || ext == "glb" || ext == "fbx" || ext == "obj";
        case AssetTypeFilter::Materials:
            return ext == "mat" || ext == "material";
        case AssetTypeFilter::Audio:
            return ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac";
        case AssetTypeFilter::Shaders:
            return ext == "vs" || ext == "fs" || ext == "glsl" || ext == "hlsl" || ext == "shader";
        case AssetTypeFilter::Scenes:
            return ext == "scene";
        case AssetTypeFilter::Prefabs:
            return ext == "prefab";
        case AssetTypeFilter::All:
        default:
            return true;
    }
}

void AssetBrowser::import_asset() {
    QString filter = "All Files (*.*);;";
    filter += "Images (*.png *.jpg *.jpeg *.tga *.bmp);;";
    filter += "Models (*.gltf *.glb *.fbx *.obj);;";
    filter += "Audio (*.wav *.mp3 *.ogg *.flac);;";
    filter += "Shaders (*.vs *.fs *.glsl *.hlsl)";

    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Import Assets",
        QString(),
        filter
    );

    if (files.isEmpty()) return;

    QString dest_dir = current_path();
    int imported = 0;

    for (const QString& src : files) {
        QFileInfo info(src);
        QString dest = dest_dir + "/" + info.fileName();

        // Check if file already exists
        if (QFile::exists(dest)) {
            auto result = QMessageBox::question(
                this,
                "File Exists",
                QString("'%1' already exists. Overwrite?").arg(info.fileName()),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
            );

            if (result == QMessageBox::Cancel) break;
            if (result == QMessageBox::No) continue;

            QFile::remove(dest);
        }

        if (QFile::copy(src, dest)) {
            imported++;
        }
    }

    if (imported > 0) {
        refresh();
        emit asset_import_requested(dest_dir);
    }
}

void AssetBrowser::create_folder() {
    QString name = QInputDialog::getText(this, "New Folder", "Folder name:");
    if (name.isEmpty()) return;

    QDir dir(current_path());
    if (!dir.mkdir(name)) {
        QMessageBox::warning(this, "Error", "Failed to create folder.");
    } else {
        refresh();
    }
}

void AssetBrowser::create_material() {
    QString name = QInputDialog::getText(this, "New Material", "Material name:");
    if (name.isEmpty()) return;

    QString path = current_path() + "/" + name + ".material";

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(R"({
    "shader": "default",
    "properties": {
        "albedo": [1.0, 1.0, 1.0, 1.0],
        "metallic": 0.0,
        "roughness": 0.5
    },
    "textures": {}
})");
        file.close();
        refresh();
    } else {
        QMessageBox::warning(this, "Error", "Failed to create material file.");
    }
}

void AssetBrowser::create_scene() {
    QString name = QInputDialog::getText(this, "New Scene", "Scene name:");
    if (name.isEmpty()) return;

    QString path = current_path() + "/" + name + ".scene";

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(R"({
    "name": ")" + name.toUtf8() + R"(",
    "entities": []
})");
        file.close();
        refresh();
    } else {
        QMessageBox::warning(this, "Error", "Failed to create scene file.");
    }
}

void AssetBrowser::create_prefab() {
    QString name = QInputDialog::getText(this, "New Prefab", "Prefab name:");
    if (name.isEmpty()) return;

    QString path = current_path() + "/" + name + ".prefab";

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(R"({
    "name": ")" + name.toUtf8() + R"(",
    "components": []
})");
        file.close();
        refresh();
    } else {
        QMessageBox::warning(this, "Error", "Failed to create prefab file.");
    }
}

void AssetBrowser::rename_selected() {
    auto indexes = m_file_list->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) return;

    auto index = indexes.first();
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
    auto indexes = m_file_list->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) return;

    // Collect unique file paths (model might return multiple indexes per item)
    QSet<QString> paths;
    for (const auto& index : indexes) {
        paths.insert(m_file_model->filePath(index));
    }

    QString message = paths.size() == 1
        ? QString("Delete '%1'?").arg(QFileInfo(*paths.begin()).fileName())
        : QString("Delete %1 items?").arg(paths.size());

    auto result = QMessageBox::question(this, "Delete", message,
                                         QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        for (const QString& path : paths) {
            QFileInfo info(path);
            if (info.isDir()) {
                QDir(path).removeRecursively();
            } else {
                QFile::remove(path);
            }
        }
        refresh();
    }
}

void AssetBrowser::show_in_explorer() {
    QString path = current_path();
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void AssetBrowser::refresh() {
    QString path = current_path();
    m_folder_model->setRootPath("");
    m_folder_model->setRootPath(m_root_path);
    m_folder_tree->setRootIndex(m_folder_model->index(m_root_path));
    update_file_view(path);
}

QIcon AssetBrowser::icon_for_file(const QString& path) const {
    // TODO: Return custom icons based on file type (Phase 3)
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
    } else if (ext == "vs" || ext == "fs" || ext == "glsl" || ext == "hlsl" || ext == "shader") {
        return "Shader";
    } else if (ext == "mat" || ext == "material") {
        return "Material";
    } else if (ext == "scene") {
        return "Scene";
    } else if (ext == "prefab") {
        return "Prefab";
    }

    return "Unknown";
}

bool AssetBrowser::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_file_list && event->type() == QEvent::KeyPress) {
        auto* key_event = static_cast<QKeyEvent*>(event);
        if (handle_key_press(key_event)) {
            return true;
        }
    }
    return QDockWidget::eventFilter(obj, event);
}

bool AssetBrowser::handle_key_press(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            // Open selected folder or emit double-click for file
            auto index = m_file_list->currentIndex();
            if (index.isValid()) {
                QString path = m_file_model->filePath(index);
                QFileInfo info(path);
                if (info.isDir()) {
                    navigate_to(path);
                } else {
                    emit asset_double_clicked(path);
                }
                return true;
            }
            break;
        }

        case Qt::Key_Backspace: {
            // Navigate to parent directory
            QString current = current_path();
            QDir dir(current);
            if (dir.cdUp() && dir.absolutePath().startsWith(m_root_path)) {
                navigate_to(dir.absolutePath());
                return true;
            }
            break;
        }

        case Qt::Key_F2: {
            // Rename selected
            rename_selected();
            return true;
        }

        case Qt::Key_Delete: {
            // Delete selected
            delete_selected();
            return true;
        }

        case Qt::Key_F5: {
            // Refresh
            refresh();
            return true;
        }

        case Qt::Key_A: {
            // Select all with Ctrl+A
            if (event->modifiers() & Qt::ControlModifier) {
                m_file_list->selectAll();
                return true;
            }
            break;
        }

        case Qt::Key_F: {
            // Focus search with Ctrl+F
            if (event->modifiers() & Qt::ControlModifier) {
                m_search_box->setFocus();
                m_search_box->selectAll();
                return true;
            }
            break;
        }

        default:
            break;
    }

    return false;
}

} // namespace editor

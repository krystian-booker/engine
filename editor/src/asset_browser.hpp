#pragma once

#include "editor_state.hpp"
#include "asset_item_model.hpp"
#include <QDockWidget>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QString>
#include <QLineEdit>
#include <QComboBox>
#include <QSlider>
#include <QToolButton>
#include <QLabel>
#include <QSet>
#include <QSettings>

namespace editor {

class BreadcrumbBar;
class AssetItemModel;
class AssetItemDelegate;
class ThumbnailCache;
class ThumbnailGenerator;

// Asset type categories for filtering
enum class AssetTypeFilter {
    All,
    Textures,
    Meshes,
    Materials,
    Audio,
    Shaders,
    Scenes,
    Prefabs
};

// Asset browser panel for file navigation and asset management
class AssetBrowser : public QDockWidget {
    Q_OBJECT

public:
    explicit AssetBrowser(EditorState* state, QWidget* parent = nullptr);
    ~AssetBrowser() override;

    // Set the root asset directory
    void set_root_path(const QString& path);
    QString root_path() const { return m_root_path; }

    // Current directory
    QString current_path() const;

    // View settings
    int icon_size() const { return m_icon_size; }
    void set_icon_size(int size);

signals:
    void asset_selected(const QString& path);
    void asset_double_clicked(const QString& path);
    void asset_import_requested(const QString& path);

private slots:
    void on_folder_selected(const QModelIndex& index);
    void on_asset_selected(const QModelIndex& index);
    void on_asset_double_clicked(const QModelIndex& index);
    void on_context_menu(const QPoint& pos);
    void on_breadcrumb_clicked(const QString& path);

    // Toolbar actions
    void on_search_changed(const QString& text);
    void on_filter_changed(int index);
    void on_icon_size_changed(int value);
    void on_view_mode_toggled();

    // File operations
    void import_asset();
    void create_folder();
    void create_material();
    void create_scene();
    void create_prefab();
    void rename_selected();
    void delete_selected();
    void show_in_explorer();
    void refresh();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setup_ui();
    void setup_toolbar();
    void setup_connections();
    void update_file_view(const QString& path);
    void apply_filters();
    void navigate_to(const QString& path);
    bool handle_key_press(QKeyEvent* event);

    // Get asset type icon based on extension
    QIcon icon_for_file(const QString& path) const;
    QString asset_type(const QString& path) const;
    bool matches_filter(const QString& path) const;

    EditorState* m_state;
    QString m_root_path;
    QString m_search_text;
    AssetTypeFilter m_type_filter = AssetTypeFilter::All;
    int m_icon_size = 64;
    bool m_list_mode = false;

    // Toolbar widgets
    QLineEdit* m_search_box;
    QComboBox* m_filter_combo;
    QSlider* m_size_slider;
    QToolButton* m_view_mode_btn;
    QToolButton* m_create_btn;
    QLabel* m_size_label;
    BreadcrumbBar* m_breadcrumb_bar;

    // UI components
    QSplitter* m_splitter;
    QTreeView* m_folder_tree;
    QListView* m_file_list;
    QFileSystemModel* m_folder_model;
    QFileSystemModel* m_file_model;

    // Custom model and thumbnail system (for future full integration)
    AssetItemModel* m_asset_model = nullptr;
    AssetItemDelegate* m_item_delegate = nullptr;
    ThumbnailCache* m_thumbnail_cache = nullptr;
    ThumbnailGenerator* m_thumbnail_generator = nullptr;

    // Favorites
    QSet<QString> m_favorites;

    QMenu* m_context_menu;
    QMenu* m_create_menu;
};

} // namespace editor

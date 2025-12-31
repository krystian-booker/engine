#pragma once

#include "editor_state.hpp"
#include <QDockWidget>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QString>

namespace editor {

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

signals:
    void asset_selected(const QString& path);
    void asset_double_clicked(const QString& path);
    void asset_import_requested(const QString& path);

private slots:
    void on_folder_selected(const QModelIndex& index);
    void on_asset_selected(const QModelIndex& index);
    void on_asset_double_clicked(const QModelIndex& index);
    void on_context_menu(const QPoint& pos);

    void import_asset();
    void create_folder();
    void rename_selected();
    void delete_selected();
    void show_in_explorer();
    void refresh();

private:
    void setup_ui();
    void setup_connections();
    void update_file_view(const QString& path);

    // Get asset type icon based on extension
    QIcon icon_for_file(const QString& path) const;
    QString asset_type(const QString& path) const;

    EditorState* m_state;
    QString m_root_path;

    // UI components
    QSplitter* m_splitter;
    QTreeView* m_folder_tree;
    QListView* m_file_list;
    QFileSystemModel* m_folder_model;
    QFileSystemModel* m_file_model;

    QMenu* m_context_menu;
};

} // namespace editor

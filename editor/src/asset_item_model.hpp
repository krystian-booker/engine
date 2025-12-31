#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QDateTime>
#include <QIcon>
#include <QMimeData>
#include <QString>
#include <vector>

namespace editor {

// Asset type enumeration
enum class AssetType {
    Unknown,
    Folder,
    Texture,
    Mesh,
    Material,
    Audio,
    Shader,
    Scene,
    Prefab
};

// Thumbnail generation state
enum class ThumbnailState {
    NotGenerated,
    Generating,
    Ready,
    Failed
};

// Single asset item data
struct AssetItem {
    QString path;
    QString name;
    AssetType type = AssetType::Unknown;
    qint64 size = 0;
    QDateTime modified;
    QIcon thumbnail;
    ThumbnailState thumb_state = ThumbnailState::NotGenerated;
    bool is_favorite = false;
};

// Custom model for asset items with filtering and drag-drop support
class AssetItemModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        TypeRole,
        SizeRole,
        ModifiedRole,
        ThumbnailStateRole,
        IsFavoriteRole
    };

    explicit AssetItemModel(QObject* parent = nullptr);
    ~AssetItemModel() override;

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Drag and drop
    Qt::DropActions supportedDragActions() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    QStringList mimeTypes() const override;

    // Path management
    void set_root_path(const QString& path);
    QString root_path() const { return m_root_path; }
    void refresh();

    // Filtering
    void set_name_filter(const QString& filter);
    void set_type_filter(AssetType type);
    QString name_filter() const { return m_name_filter; }
    AssetType type_filter() const { return m_type_filter; }

    // Item access
    const AssetItem* item_at(int row) const;
    const AssetItem* item_at(const QModelIndex& index) const;
    int index_of(const QString& path) const;

    // Thumbnail updates
    void set_thumbnail(const QString& path, const QIcon& icon);
    void set_thumbnail_state(const QString& path, ThumbnailState state);

    // Favorites
    void toggle_favorite(const QString& path);
    bool is_favorite(const QString& path) const;

    // Static helpers
    static AssetType type_from_extension(const QString& ext);
    static QString type_to_string(AssetType type);
    static QString asset_type_to_mime(AssetType type);

signals:
    void thumbnail_needed(const QString& path, AssetType type);
    void directory_changed();

private slots:
    void on_directory_changed(const QString& path);

private:
    void scan_directory();
    void apply_filters();
    bool matches_filter(const AssetItem& item) const;

    QString m_root_path;
    QString m_name_filter;
    AssetType m_type_filter = AssetType::Unknown; // Unknown = All types

    std::vector<AssetItem> m_all_items;      // All items in directory
    std::vector<AssetItem> m_filtered_items; // Items after filtering

    QFileSystemWatcher* m_watcher;
    QSet<QString> m_favorites;
};

} // namespace editor

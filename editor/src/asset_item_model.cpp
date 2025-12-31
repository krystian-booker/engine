#include "asset_item_model.hpp"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <algorithm>

namespace editor {

AssetItemModel::AssetItemModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_watcher(new QFileSystemWatcher(this))
{
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &AssetItemModel::on_directory_changed);
}

AssetItemModel::~AssetItemModel() = default;

int AssetItemModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_filtered_items.size());
}

QVariant AssetItemModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(m_filtered_items.size())) {
        return QVariant();
    }

    const auto& item = m_filtered_items[index.row()];

    switch (role) {
        case Qt::DisplayRole:
            return item.name;

        case Qt::DecorationRole:
            if (item.thumb_state == ThumbnailState::Ready && !item.thumbnail.isNull()) {
                return item.thumbnail;
            }
            // Return default icon based on type
            return QVariant();

        case Qt::ToolTipRole: {
            QString tooltip = QString("<b>%1</b><br>").arg(item.name);
            tooltip += QString("Type: %1<br>").arg(type_to_string(item.type));
            tooltip += QString("Size: %1<br>").arg(
                item.size < 1024 ? QString("%1 B").arg(item.size) :
                item.size < 1024 * 1024 ? QString("%1 KB").arg(item.size / 1024) :
                QString("%1 MB").arg(item.size / (1024 * 1024))
            );
            tooltip += QString("Modified: %1").arg(item.modified.toString("yyyy-MM-dd hh:mm"));
            return tooltip;
        }

        case PathRole:
            return item.path;

        case TypeRole:
            return static_cast<int>(item.type);

        case SizeRole:
            return item.size;

        case ModifiedRole:
            return item.modified;

        case ThumbnailStateRole:
            return static_cast<int>(item.thumb_state);

        case IsFavoriteRole:
            return item.is_favorite;

        default:
            return QVariant();
    }
}

Qt::ItemFlags AssetItemModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags default_flags = QAbstractListModel::flags(index);

    if (index.isValid()) {
        return default_flags | Qt::ItemIsDragEnabled;
    }

    return default_flags;
}

Qt::DropActions AssetItemModel::supportedDragActions() const {
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList AssetItemModel::mimeTypes() const {
    return { "application/x-engine-asset", "text/uri-list" };
}

QMimeData* AssetItemModel::mimeData(const QModelIndexList& indexes) const {
    auto* mime_data = new QMimeData();

    QJsonArray assets;
    QList<QUrl> urls;

    for (const auto& index : indexes) {
        if (!index.isValid()) continue;

        const auto& item = m_filtered_items[index.row()];

        QJsonObject obj;
        obj["path"] = item.path;
        obj["type"] = type_to_string(item.type);
        assets.append(obj);

        urls << QUrl::fromLocalFile(item.path);
    }

    // Custom MIME type for internal drag-drop
    QJsonDocument doc(assets);
    mime_data->setData("application/x-engine-asset", doc.toJson(QJsonDocument::Compact));

    // Standard URLs for external apps
    mime_data->setUrls(urls);

    return mime_data;
}

void AssetItemModel::set_root_path(const QString& path) {
    if (m_root_path == path) return;

    // Stop watching old path
    if (!m_root_path.isEmpty()) {
        m_watcher->removePath(m_root_path);
    }

    m_root_path = path;

    // Start watching new path
    if (!m_root_path.isEmpty() && QDir(m_root_path).exists()) {
        m_watcher->addPath(m_root_path);
    }

    scan_directory();
}

void AssetItemModel::refresh() {
    scan_directory();
}

void AssetItemModel::set_name_filter(const QString& filter) {
    if (m_name_filter == filter) return;
    m_name_filter = filter;
    apply_filters();
}

void AssetItemModel::set_type_filter(AssetType type) {
    if (m_type_filter == type) return;
    m_type_filter = type;
    apply_filters();
}

const AssetItem* AssetItemModel::item_at(int row) const {
    if (row < 0 || row >= static_cast<int>(m_filtered_items.size())) {
        return nullptr;
    }
    return &m_filtered_items[row];
}

const AssetItem* AssetItemModel::item_at(const QModelIndex& index) const {
    return item_at(index.row());
}

int AssetItemModel::index_of(const QString& path) const {
    for (size_t i = 0; i < m_filtered_items.size(); ++i) {
        if (m_filtered_items[i].path == path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void AssetItemModel::set_thumbnail(const QString& path, const QIcon& icon) {
    // Update in all items
    for (auto& item : m_all_items) {
        if (item.path == path) {
            item.thumbnail = icon;
            item.thumb_state = ThumbnailState::Ready;
            break;
        }
    }

    // Update in filtered items and emit change
    for (size_t i = 0; i < m_filtered_items.size(); ++i) {
        if (m_filtered_items[i].path == path) {
            m_filtered_items[i].thumbnail = icon;
            m_filtered_items[i].thumb_state = ThumbnailState::Ready;
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, { Qt::DecorationRole, ThumbnailStateRole });
            break;
        }
    }
}

void AssetItemModel::set_thumbnail_state(const QString& path, ThumbnailState state) {
    for (auto& item : m_all_items) {
        if (item.path == path) {
            item.thumb_state = state;
            break;
        }
    }

    for (size_t i = 0; i < m_filtered_items.size(); ++i) {
        if (m_filtered_items[i].path == path) {
            m_filtered_items[i].thumb_state = state;
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, { ThumbnailStateRole });
            break;
        }
    }
}

void AssetItemModel::toggle_favorite(const QString& path) {
    if (m_favorites.contains(path)) {
        m_favorites.remove(path);
    } else {
        m_favorites.insert(path);
    }

    // Update items
    for (auto& item : m_all_items) {
        if (item.path == path) {
            item.is_favorite = m_favorites.contains(path);
            break;
        }
    }

    for (size_t i = 0; i < m_filtered_items.size(); ++i) {
        if (m_filtered_items[i].path == path) {
            m_filtered_items[i].is_favorite = m_favorites.contains(path);
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, { IsFavoriteRole });
            break;
        }
    }
}

bool AssetItemModel::is_favorite(const QString& path) const {
    return m_favorites.contains(path);
}

void AssetItemModel::on_directory_changed(const QString& path) {
    Q_UNUSED(path)
    scan_directory();
    emit directory_changed();
}

void AssetItemModel::scan_directory() {
    beginResetModel();

    m_all_items.clear();

    if (m_root_path.isEmpty()) {
        m_filtered_items.clear();
        endResetModel();
        return;
    }

    QDir dir(m_root_path);
    if (!dir.exists()) {
        m_filtered_items.clear();
        endResetModel();
        return;
    }

    // Get all files
    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase
    );

    for (const QFileInfo& info : entries) {
        AssetItem item;
        item.path = info.absoluteFilePath();
        item.name = info.fileName();
        item.type = type_from_extension(info.suffix().toLower());
        item.size = info.size();
        item.modified = info.lastModified();
        item.is_favorite = m_favorites.contains(item.path);
        item.thumb_state = ThumbnailState::NotGenerated;

        m_all_items.push_back(std::move(item));
    }

    apply_filters();
    endResetModel();

    // Request thumbnails for visible items
    for (const auto& item : m_filtered_items) {
        if (item.thumb_state == ThumbnailState::NotGenerated) {
            emit thumbnail_needed(item.path, item.type);
        }
    }
}

void AssetItemModel::apply_filters() {
    m_filtered_items.clear();

    for (const auto& item : m_all_items) {
        if (matches_filter(item)) {
            m_filtered_items.push_back(item);
        }
    }
}

bool AssetItemModel::matches_filter(const AssetItem& item) const {
    // Type filter
    if (m_type_filter != AssetType::Unknown && item.type != m_type_filter) {
        return false;
    }

    // Name filter
    if (!m_name_filter.isEmpty()) {
        if (!item.name.contains(m_name_filter, Qt::CaseInsensitive)) {
            return false;
        }
    }

    return true;
}

AssetType AssetItemModel::type_from_extension(const QString& ext) {
    QString lower = ext.toLower();

    if (lower == "png" || lower == "jpg" || lower == "jpeg" ||
        lower == "tga" || lower == "bmp" || lower == "hdr") {
        return AssetType::Texture;
    }
    if (lower == "gltf" || lower == "glb" || lower == "fbx" || lower == "obj") {
        return AssetType::Mesh;
    }
    if (lower == "mat" || lower == "material") {
        return AssetType::Material;
    }
    if (lower == "wav" || lower == "mp3" || lower == "ogg" || lower == "flac") {
        return AssetType::Audio;
    }
    if (lower == "vs" || lower == "fs" || lower == "glsl" ||
        lower == "hlsl" || lower == "shader") {
        return AssetType::Shader;
    }
    if (lower == "scene") {
        return AssetType::Scene;
    }
    if (lower == "prefab") {
        return AssetType::Prefab;
    }

    return AssetType::Unknown;
}

QString AssetItemModel::type_to_string(AssetType type) {
    switch (type) {
        case AssetType::Folder:   return "Folder";
        case AssetType::Texture:  return "Texture";
        case AssetType::Mesh:     return "Mesh";
        case AssetType::Material: return "Material";
        case AssetType::Audio:    return "Audio";
        case AssetType::Shader:   return "Shader";
        case AssetType::Scene:    return "Scene";
        case AssetType::Prefab:   return "Prefab";
        default:                  return "Unknown";
    }
}

QString AssetItemModel::asset_type_to_mime(AssetType type) {
    switch (type) {
        case AssetType::Texture:  return "image/*";
        case AssetType::Mesh:     return "model/gltf+json";
        case AssetType::Audio:    return "audio/*";
        default:                  return "application/octet-stream";
    }
}

} // namespace editor

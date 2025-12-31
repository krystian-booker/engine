#pragma once

#include <QObject>
#include <QPixmap>
#include <QHash>
#include <QString>
#include <QDateTime>
#include <functional>

namespace editor {

// Entry in the thumbnail cache
struct ThumbnailEntry {
    QPixmap pixmap;
    QDateTime access_time;
    qint64 file_modified;
};

// LRU cache for asset thumbnails with disk persistence
class ThumbnailCache : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailCache(QObject* parent = nullptr);
    ~ThumbnailCache() override;

    // Initialize cache directory
    void set_cache_dir(const QString& dir);
    QString cache_dir() const { return m_cache_dir; }

    // Get cached thumbnail (returns null if not cached)
    QPixmap get(const QString& path, int size);

    // Store thumbnail in cache
    void put(const QString& path, int size, const QPixmap& pixmap);

    // Check if thumbnail is cached and up-to-date
    bool has_valid(const QString& path, int size);

    // Invalidate cache for a specific file
    void invalidate(const QString& path);

    // Clear all cache
    void clear();

    // Cache size management
    void set_max_entries(int count);
    int max_entries() const { return m_max_entries; }
    int entry_count() const { return m_cache.size(); }

    // Disk cache operations
    void save_to_disk(const QString& path, int size, const QPixmap& pixmap);
    QPixmap load_from_disk(const QString& path, int size);

private:
    QString cache_key(const QString& path, int size) const;
    QString disk_cache_path(const QString& path, int size) const;
    void evict_oldest();

    QString m_cache_dir;
    int m_max_entries = 500;

    QHash<QString, ThumbnailEntry> m_cache;
};

} // namespace editor

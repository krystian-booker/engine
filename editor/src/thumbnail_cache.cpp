#include "thumbnail_cache.hpp"
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <algorithm>

namespace editor {

ThumbnailCache::ThumbnailCache(QObject* parent)
    : QObject(parent)
{
}

ThumbnailCache::~ThumbnailCache() = default;

void ThumbnailCache::set_cache_dir(const QString& dir) {
    m_cache_dir = dir;

    QDir cache_dir(dir);
    if (!cache_dir.exists()) {
        cache_dir.mkpath(".");
    }
}

QPixmap ThumbnailCache::get(const QString& path, int size) {
    QString key = cache_key(path, size);

    // Check memory cache first
    if (m_cache.contains(key)) {
        auto& entry = m_cache[key];

        // Check if file was modified
        QFileInfo info(path);
        if (info.exists() && info.lastModified().toSecsSinceEpoch() == entry.file_modified) {
            entry.access_time = QDateTime::currentDateTime();
            return entry.pixmap;
        } else {
            // File changed, invalidate
            m_cache.remove(key);
        }
    }

    // Try disk cache
    QPixmap pixmap = load_from_disk(path, size);
    if (!pixmap.isNull()) {
        // Store in memory cache
        ThumbnailEntry entry;
        entry.pixmap = pixmap;
        entry.access_time = QDateTime::currentDateTime();
        entry.file_modified = QFileInfo(path).lastModified().toSecsSinceEpoch();
        m_cache[key] = entry;
        return pixmap;
    }

    return QPixmap();
}

void ThumbnailCache::put(const QString& path, int size, const QPixmap& pixmap) {
    QString key = cache_key(path, size);

    // Evict if at capacity
    while (m_cache.size() >= m_max_entries) {
        evict_oldest();
    }

    ThumbnailEntry entry;
    entry.pixmap = pixmap;
    entry.access_time = QDateTime::currentDateTime();
    entry.file_modified = QFileInfo(path).lastModified().toSecsSinceEpoch();

    m_cache[key] = entry;

    // Also save to disk
    save_to_disk(path, size, pixmap);
}

bool ThumbnailCache::has_valid(const QString& path, int size) {
    QString key = cache_key(path, size);

    if (m_cache.contains(key)) {
        QFileInfo info(path);
        return info.exists() &&
               info.lastModified().toSecsSinceEpoch() == m_cache[key].file_modified;
    }

    // Check disk cache
    QString disk_path = disk_cache_path(path, size);
    if (QFile::exists(disk_path)) {
        QFileInfo cache_info(disk_path);
        QFileInfo source_info(path);
        return source_info.exists() &&
               cache_info.lastModified() > source_info.lastModified();
    }

    return false;
}

void ThumbnailCache::invalidate(const QString& path) {
    // Remove all sizes from memory cache
    QList<QString> keys_to_remove;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it.key().startsWith(path + "_")) {
            keys_to_remove.append(it.key());
        }
    }
    for (const QString& key : keys_to_remove) {
        m_cache.remove(key);
    }

    // Remove from disk cache
    if (!m_cache_dir.isEmpty()) {
        QDir dir(m_cache_dir);
        QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex();
        QStringList files = dir.entryList({ QString(hash) + "_*.png" }, QDir::Files);
        for (const QString& file : files) {
            dir.remove(file);
        }
    }
}

void ThumbnailCache::clear() {
    m_cache.clear();

    if (!m_cache_dir.isEmpty()) {
        QDir dir(m_cache_dir);
        QStringList files = dir.entryList({ "*.png" }, QDir::Files);
        for (const QString& file : files) {
            dir.remove(file);
        }
    }
}

void ThumbnailCache::set_max_entries(int count) {
    m_max_entries = qMax(10, count);

    while (m_cache.size() > m_max_entries) {
        evict_oldest();
    }
}

void ThumbnailCache::save_to_disk(const QString& path, int size, const QPixmap& pixmap) {
    if (m_cache_dir.isEmpty()) return;

    QString disk_path = disk_cache_path(path, size);
    pixmap.save(disk_path, "PNG");
}

QPixmap ThumbnailCache::load_from_disk(const QString& path, int size) {
    if (m_cache_dir.isEmpty()) return QPixmap();

    QString disk_path = disk_cache_path(path, size);
    if (!QFile::exists(disk_path)) return QPixmap();

    // Check if source file is newer than cache
    QFileInfo cache_info(disk_path);
    QFileInfo source_info(path);

    if (source_info.exists() && source_info.lastModified() > cache_info.lastModified()) {
        // Cache is stale
        QFile::remove(disk_path);
        return QPixmap();
    }

    return QPixmap(disk_path);
}

QString ThumbnailCache::cache_key(const QString& path, int size) const {
    return QString("%1_%2").arg(path).arg(size);
}

QString ThumbnailCache::disk_cache_path(const QString& path, int size) const {
    QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex();
    return QString("%1/%2_%3.png").arg(m_cache_dir).arg(QString(hash)).arg(size);
}

void ThumbnailCache::evict_oldest() {
    if (m_cache.isEmpty()) return;

    QString oldest_key;
    QDateTime oldest_time = QDateTime::currentDateTime();

    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it.value().access_time < oldest_time) {
            oldest_time = it.value().access_time;
            oldest_key = it.key();
        }
    }

    if (!oldest_key.isEmpty()) {
        m_cache.remove(oldest_key);
    }
}

} // namespace editor

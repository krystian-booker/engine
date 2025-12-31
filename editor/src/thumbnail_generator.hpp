#pragma once

#include "asset_item_model.hpp"
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QPixmap>
#include <QQueue>

namespace editor {

class ThumbnailCache;

// Request for thumbnail generation
struct ThumbnailRequest {
    QString path;
    AssetType type;
    int size;
};

// Worker thread for generating thumbnails
class ThumbnailWorker : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailWorker(QObject* parent = nullptr);

public slots:
    void process_request(const ThumbnailRequest& request);

signals:
    void thumbnail_ready(const QString& path, const QPixmap& pixmap, int size);
    void thumbnail_failed(const QString& path);

private:
    QPixmap generate_texture_thumbnail(const QString& path, int size);
    QPixmap generate_default_thumbnail(AssetType type, int size);
};

// Manager for asynchronous thumbnail generation
class ThumbnailGenerator : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailGenerator(QObject* parent = nullptr);
    ~ThumbnailGenerator() override;

    // Set the cache to use
    void set_cache(ThumbnailCache* cache);

    // Request thumbnail generation
    void request(const QString& path, AssetType type, int size = 64);

    // Cancel pending requests
    void cancel_all();

    // Check if generation is in progress for a path
    bool is_generating(const QString& path) const;

signals:
    void thumbnail_ready(const QString& path, const QIcon& icon);
    void thumbnail_failed(const QString& path);

private slots:
    void on_thumbnail_ready(const QString& path, const QPixmap& pixmap, int size);
    void on_thumbnail_failed(const QString& path);

private:
    void process_next();

    ThumbnailCache* m_cache = nullptr;
    QThread m_worker_thread;
    ThumbnailWorker* m_worker;

    QQueue<ThumbnailRequest> m_pending;
    QSet<QString> m_in_progress;
    mutable QMutex m_mutex;
    bool m_processing = false;
};

} // namespace editor

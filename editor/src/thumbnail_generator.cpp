#include "thumbnail_generator.hpp"
#include "thumbnail_cache.hpp"
#include <QImage>
#include <QPainter>
#include <QFileInfo>

namespace editor {

// ThumbnailWorker implementation

ThumbnailWorker::ThumbnailWorker(QObject* parent)
    : QObject(parent)
{
}

void ThumbnailWorker::process_request(const ThumbnailRequest& request) {
    QPixmap pixmap;

    switch (request.type) {
        case AssetType::Texture:
            pixmap = generate_texture_thumbnail(request.path, request.size);
            break;

        case AssetType::Mesh:
        case AssetType::Material:
        case AssetType::Audio:
        case AssetType::Shader:
        case AssetType::Scene:
        case AssetType::Prefab:
        default:
            pixmap = generate_default_thumbnail(request.type, request.size);
            break;
    }

    if (!pixmap.isNull()) {
        emit thumbnail_ready(request.path, pixmap, request.size);
    } else {
        emit thumbnail_failed(request.path);
    }
}

QPixmap ThumbnailWorker::generate_texture_thumbnail(const QString& path, int size) {
    QImage image(path);
    if (image.isNull()) {
        return generate_default_thumbnail(AssetType::Texture, size);
    }

    // Scale to thumbnail size preserving aspect ratio
    QImage scaled = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Create centered thumbnail on transparent background
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw checkerboard for transparency
    int check_size = 8;
    for (int y = 0; y < size; y += check_size) {
        for (int x = 0; x < size; x += check_size) {
            bool dark = ((x / check_size) + (y / check_size)) % 2;
            painter.fillRect(x, y, check_size, check_size,
                           dark ? QColor(60, 60, 60) : QColor(80, 80, 80));
        }
    }

    // Draw centered image
    int x = (size - scaled.width()) / 2;
    int y = (size - scaled.height()) / 2;
    painter.drawImage(x, y, scaled);

    // Draw border
    painter.setPen(QColor(100, 100, 100));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(0, 0, size - 1, size - 1);

    return pixmap;
}

QPixmap ThumbnailWorker::generate_default_thumbnail(AssetType type, int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor color;
    QString letter;

    switch (type) {
        case AssetType::Texture:
            color = QColor(100, 180, 100);
            letter = "T";
            break;
        case AssetType::Mesh:
            color = QColor(100, 150, 200);
            letter = "M";
            break;
        case AssetType::Material:
            color = QColor(200, 100, 150);
            letter = "Mt";
            break;
        case AssetType::Audio:
            color = QColor(200, 180, 100);
            letter = "A";
            break;
        case AssetType::Shader:
            color = QColor(150, 100, 200);
            letter = "S";
            break;
        case AssetType::Scene:
            color = QColor(100, 200, 200);
            letter = "Sc";
            break;
        case AssetType::Prefab:
            color = QColor(200, 150, 100);
            letter = "P";
            break;
        default:
            color = QColor(128, 128, 128);
            letter = "?";
            break;
    }

    // Draw rounded rectangle background
    int margin = size / 8;
    QRect rect(margin, margin, size - margin * 2, size - margin * 2);

    painter.setBrush(color);
    painter.setPen(color.darker(120));
    painter.drawRoundedRect(rect, size / 8, size / 8);

    // Draw letter
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(size / 3);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, letter);

    return pixmap;
}

// ThumbnailGenerator implementation

ThumbnailGenerator::ThumbnailGenerator(QObject* parent)
    : QObject(parent)
    , m_worker(new ThumbnailWorker())
{
    m_worker->moveToThread(&m_worker_thread);

    connect(&m_worker_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &ThumbnailGenerator::thumbnail_ready, this, [](const QString&, const QIcon&) {});

    // Use queued connection for cross-thread signals
    connect(m_worker, &ThumbnailWorker::thumbnail_ready,
            this, &ThumbnailGenerator::on_thumbnail_ready, Qt::QueuedConnection);
    connect(m_worker, &ThumbnailWorker::thumbnail_failed,
            this, &ThumbnailGenerator::on_thumbnail_failed, Qt::QueuedConnection);

    m_worker_thread.start();
}

ThumbnailGenerator::~ThumbnailGenerator() {
    cancel_all();
    m_worker_thread.quit();
    m_worker_thread.wait();
}

void ThumbnailGenerator::set_cache(ThumbnailCache* cache) {
    m_cache = cache;
}

void ThumbnailGenerator::request(const QString& path, AssetType type, int size) {
    QMutexLocker locker(&m_mutex);

    // Check if already in progress
    if (m_in_progress.contains(path)) {
        return;
    }

    // Check cache first
    if (m_cache && m_cache->has_valid(path, size)) {
        QPixmap pixmap = m_cache->get(path, size);
        if (!pixmap.isNull()) {
            emit thumbnail_ready(path, QIcon(pixmap));
            return;
        }
    }

    // Queue request
    ThumbnailRequest request;
    request.path = path;
    request.type = type;
    request.size = size;

    m_pending.enqueue(request);
    m_in_progress.insert(path);

    locker.unlock();

    process_next();
}

void ThumbnailGenerator::cancel_all() {
    QMutexLocker locker(&m_mutex);
    m_pending.clear();
    m_in_progress.clear();
}

bool ThumbnailGenerator::is_generating(const QString& path) const {
    QMutexLocker locker(&m_mutex);
    return m_in_progress.contains(path);
}

void ThumbnailGenerator::on_thumbnail_ready(const QString& path, const QPixmap& pixmap, int size) {
    {
        QMutexLocker locker(&m_mutex);
        m_in_progress.remove(path);
        m_processing = false;
    }

    // Store in cache
    if (m_cache) {
        m_cache->put(path, size, pixmap);
    }

    emit thumbnail_ready(path, QIcon(pixmap));

    process_next();
}

void ThumbnailGenerator::on_thumbnail_failed(const QString& path) {
    {
        QMutexLocker locker(&m_mutex);
        m_in_progress.remove(path);
        m_processing = false;
    }

    emit thumbnail_failed(path);

    process_next();
}

void ThumbnailGenerator::process_next() {
    QMutexLocker locker(&m_mutex);

    if (m_processing || m_pending.isEmpty()) {
        return;
    }

    ThumbnailRequest request = m_pending.dequeue();
    m_processing = true;

    locker.unlock();

    // Process on worker thread
    QMetaObject::invokeMethod(m_worker, [this, request]() {
        m_worker->process_request(request);
    }, Qt::QueuedConnection);
}

} // namespace editor

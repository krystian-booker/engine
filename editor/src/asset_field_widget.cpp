#include "asset_field_widget.hpp"
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QPainter>
#include <QFileDialog>

namespace editor {

AssetFieldWidget::AssetFieldWidget(QWidget* parent)
    : QWidget(parent)
{
    setup_ui();
}

AssetFieldWidget::AssetFieldWidget(AssetType accepted_type, QWidget* parent)
    : QWidget(parent)
    , m_accepted_type(accepted_type)
{
    setup_ui();
}

AssetFieldWidget::~AssetFieldWidget() = default;

void AssetFieldWidget::setup_ui() {
    setAcceptDrops(true);
    setMinimumHeight(24);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Icon label
    m_icon_label = new QLabel(this);
    m_icon_label->setFixedSize(20, 20);
    m_icon_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_icon_label);

    // Path display/edit
    m_path_edit = new QLineEdit(this);
    m_path_edit->setReadOnly(true);
    m_path_edit->setPlaceholderText("None (Drop asset here)");
    m_path_edit->setStyleSheet(R"(
        QLineEdit {
            background: #3C3C3C;
            border: 1px solid #555;
            border-radius: 2px;
            padding: 2px 4px;
            color: #DDD;
        }
        QLineEdit:focus {
            border-color: #0078D4;
        }
    )");
    layout->addWidget(m_path_edit, 1);

    // Browse button
    m_browse_btn = new QPushButton("...", this);
    m_browse_btn->setFixedWidth(24);
    m_browse_btn->setToolTip("Browse for asset");
    connect(m_browse_btn, &QPushButton::clicked, this, &AssetFieldWidget::on_browse_clicked);
    layout->addWidget(m_browse_btn);

    // Clear button
    m_clear_btn = new QPushButton("X", this);
    m_clear_btn->setFixedWidth(20);
    m_clear_btn->setToolTip("Clear");
    m_clear_btn->setVisible(false);
    connect(m_clear_btn, &QPushButton::clicked, this, &AssetFieldWidget::on_clear_clicked);
    layout->addWidget(m_clear_btn);

    setStyleSheet(R"(
        QPushButton {
            background: #555;
            border: 1px solid #666;
            border-radius: 2px;
            color: #DDD;
            padding: 2px;
        }
        QPushButton:hover {
            background: #666;
        }
        QPushButton:pressed {
            background: #444;
        }
    )");
}

void AssetFieldWidget::set_asset(const QString& path) {
    m_asset_path = path;

    if (path.isEmpty()) {
        m_path_edit->clear();
        m_path_edit->setPlaceholderText("None (Drop asset here)");
        m_icon_label->clear();
        m_clear_btn->setVisible(false);
    } else {
        QFileInfo info(path);
        m_path_edit->setText(info.fileName());
        m_path_edit->setToolTip(path);
        m_clear_btn->setVisible(!m_read_only);

        // Set type icon
        AssetType type = AssetItemModel::type_from_extension(info.suffix());
        QString type_char;
        QColor color;

        switch (type) {
            case AssetType::Texture:  type_char = "T"; color = QColor(100, 180, 100); break;
            case AssetType::Mesh:     type_char = "M"; color = QColor(100, 150, 200); break;
            case AssetType::Material: type_char = "Mt"; color = QColor(200, 100, 150); break;
            case AssetType::Audio:    type_char = "A"; color = QColor(200, 180, 100); break;
            case AssetType::Shader:   type_char = "S"; color = QColor(150, 100, 200); break;
            case AssetType::Scene:    type_char = "Sc"; color = QColor(100, 200, 200); break;
            case AssetType::Prefab:   type_char = "P"; color = QColor(200, 150, 100); break;
            default:                  type_char = "?"; color = QColor(128, 128, 128); break;
        }

        QPixmap pixmap(18, 18);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(color);
        painter.setPen(color.darker(120));
        painter.drawRoundedRect(1, 1, 16, 16, 3, 3);
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(8);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, type_char);
        m_icon_label->setPixmap(pixmap);
    }
}

void AssetFieldWidget::set_accepted_type(AssetType type) {
    m_accepted_type = type;

    // Update placeholder text based on type
    QString type_name = AssetItemModel::type_to_string(type);
    if (type == AssetType::Unknown) {
        m_path_edit->setPlaceholderText("None (Drop asset here)");
    } else {
        m_path_edit->setPlaceholderText(QString("None (Drop %1 here)").arg(type_name));
    }
}

void AssetFieldWidget::set_read_only(bool read_only) {
    m_read_only = read_only;
    m_browse_btn->setEnabled(!read_only);
    m_clear_btn->setVisible(!read_only && !m_asset_path.isEmpty());
    setAcceptDrops(!read_only);
}

void AssetFieldWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_read_only) {
        event->ignore();
        return;
    }

    if (accepts_drop(event->mimeData())) {
        event->acceptProposedAction();
        m_drag_hover = true;
        update();
    } else {
        event->ignore();
    }
}

void AssetFieldWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event)
    m_drag_hover = false;
    update();
}

void AssetFieldWidget::dropEvent(QDropEvent* event) {
    if (m_read_only) {
        event->ignore();
        return;
    }

    m_drag_hover = false;
    update();

    if (accepts_drop(event->mimeData())) {
        QString path = extract_asset_path(event->mimeData());
        if (!path.isEmpty()) {
            set_asset(path);
            emit asset_changed(path);
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

void AssetFieldWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    if (m_drag_hover) {
        QPainter painter(this);
        painter.setPen(QPen(QColor(0, 120, 212), 2));
        painter.setBrush(QColor(0, 120, 212, 30));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 3, 3);
    }
}

void AssetFieldWidget::on_browse_clicked() {
    QString filter;
    QString title = "Select Asset";

    switch (m_accepted_type) {
        case AssetType::Texture:
            filter = "Images (*.png *.jpg *.jpeg *.tga *.bmp);;All Files (*.*)";
            title = "Select Texture";
            break;
        case AssetType::Mesh:
            filter = "Models (*.gltf *.glb *.fbx *.obj);;All Files (*.*)";
            title = "Select Mesh";
            break;
        case AssetType::Material:
            filter = "Materials (*.mat *.material);;All Files (*.*)";
            title = "Select Material";
            break;
        case AssetType::Audio:
            filter = "Audio (*.wav *.mp3 *.ogg *.flac);;All Files (*.*)";
            title = "Select Audio";
            break;
        case AssetType::Shader:
            filter = "Shaders (*.vs *.fs *.glsl *.hlsl *.shader);;All Files (*.*)";
            title = "Select Shader";
            break;
        case AssetType::Scene:
            filter = "Scenes (*.scene);;All Files (*.*)";
            title = "Select Scene";
            break;
        case AssetType::Prefab:
            filter = "Prefabs (*.prefab);;All Files (*.*)";
            title = "Select Prefab";
            break;
        default:
            filter = "All Files (*.*)";
            break;
    }

    QString path = QFileDialog::getOpenFileName(this, title, QString(), filter);
    if (!path.isEmpty()) {
        set_asset(path);
        emit asset_changed(path);
    }

    emit browse_requested();
}

void AssetFieldWidget::on_clear_clicked() {
    set_asset(QString());
    emit asset_changed(QString());
}

bool AssetFieldWidget::accepts_drop(const QMimeData* mime_data) const {
    if (!mime_data->hasFormat("application/x-engine-asset") &&
        !mime_data->hasUrls()) {
        return false;
    }

    // If we accept all types, any asset is fine
    if (m_accepted_type == AssetType::Unknown) {
        return true;
    }

    // Check if the dropped asset matches our accepted type
    AssetType dropped_type = extract_asset_type(mime_data);
    return dropped_type == m_accepted_type;
}

QString AssetFieldWidget::extract_asset_path(const QMimeData* mime_data) const {
    // Try custom MIME type first
    if (mime_data->hasFormat("application/x-engine-asset")) {
        QByteArray data = mime_data->data("application/x-engine-asset");
        QJsonDocument doc = QJsonDocument::fromJson(data);

        if (doc.isArray() && !doc.array().isEmpty()) {
            QJsonObject obj = doc.array().first().toObject();
            return obj["path"].toString();
        }
    }

    // Fallback to URLs
    if (mime_data->hasUrls()) {
        QList<QUrl> urls = mime_data->urls();
        if (!urls.isEmpty()) {
            return urls.first().toLocalFile();
        }
    }

    return QString();
}

AssetType AssetFieldWidget::extract_asset_type(const QMimeData* mime_data) const {
    // Try custom MIME type first
    if (mime_data->hasFormat("application/x-engine-asset")) {
        QByteArray data = mime_data->data("application/x-engine-asset");
        QJsonDocument doc = QJsonDocument::fromJson(data);

        if (doc.isArray() && !doc.array().isEmpty()) {
            QJsonObject obj = doc.array().first().toObject();
            QString type_str = obj["type"].toString();

            if (type_str == "Texture") return AssetType::Texture;
            if (type_str == "Mesh") return AssetType::Mesh;
            if (type_str == "Material") return AssetType::Material;
            if (type_str == "Audio") return AssetType::Audio;
            if (type_str == "Shader") return AssetType::Shader;
            if (type_str == "Scene") return AssetType::Scene;
            if (type_str == "Prefab") return AssetType::Prefab;
        }
    }

    // Fallback to URLs - determine type from extension
    if (mime_data->hasUrls()) {
        QList<QUrl> urls = mime_data->urls();
        if (!urls.isEmpty()) {
            QString path = urls.first().toLocalFile();
            QFileInfo info(path);
            return AssetItemModel::type_from_extension(info.suffix());
        }
    }

    return AssetType::Unknown;
}

} // namespace editor

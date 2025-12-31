#include "breadcrumb_bar.hpp"
#include <QLabel>
#include <QDir>

namespace editor {

BreadcrumbBar::BreadcrumbBar(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(2);
    m_layout->addStretch();

    setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            padding: 2px 6px;
            color: #DDD;
        }
        QPushButton:hover {
            background: #555;
            border-radius: 2px;
        }
        QLabel {
            color: #888;
            padding: 0 2px;
        }
    )");
}

BreadcrumbBar::~BreadcrumbBar() = default;

void BreadcrumbBar::set_root_path(const QString& root_path) {
    m_root_path = QDir(root_path).absolutePath();
    rebuild_breadcrumbs();
}

void BreadcrumbBar::set_path(const QString& path) {
    m_current_path = QDir(path).absolutePath();
    rebuild_breadcrumbs();
}

void BreadcrumbBar::clear_breadcrumbs() {
    for (auto* btn : m_buttons) {
        m_layout->removeWidget(btn);
        delete btn;
    }
    m_buttons.clear();

    // Remove any separator labels
    QLayoutItem* item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void BreadcrumbBar::rebuild_breadcrumbs() {
    clear_breadcrumbs();

    if (m_root_path.isEmpty() || m_current_path.isEmpty()) {
        m_layout->addStretch();
        return;
    }

    // Get relative path from root
    QDir root_dir(m_root_path);
    QString relative = root_dir.relativeFilePath(m_current_path);

    // Split into segments
    QStringList segments;
    segments << "Assets";  // Root always shows as "Assets"

    if (!relative.isEmpty() && relative != ".") {
        segments << relative.split('/', Qt::SkipEmptyParts);
    }

    // Build path progressively for each button
    QString accumulated_path = m_root_path;

    for (int i = 0; i < segments.size(); ++i) {
        // Add separator (except before first)
        if (i > 0) {
            auto* sep = new QLabel(">", this);
            m_layout->addWidget(sep);
            accumulated_path = accumulated_path + "/" + segments[i];
        }

        auto* btn = new QPushButton(segments[i], this);
        btn->setProperty("path", accumulated_path);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, &BreadcrumbBar::on_segment_clicked);

        m_layout->addWidget(btn);
        m_buttons.push_back(btn);
    }

    m_layout->addStretch();
}

void BreadcrumbBar::on_segment_clicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        QString path = btn->property("path").toString();
        emit path_clicked(path);
    }
}

} // namespace editor

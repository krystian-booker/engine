#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <vector>

namespace editor {

// Breadcrumb navigation bar for displaying and navigating folder paths
class BreadcrumbBar : public QWidget {
    Q_OBJECT

public:
    explicit BreadcrumbBar(QWidget* parent = nullptr);
    ~BreadcrumbBar() override;

    void set_path(const QString& path);
    void set_root_path(const QString& root_path);
    QString current_path() const { return m_current_path; }

signals:
    void path_clicked(const QString& path);

private slots:
    void on_segment_clicked();

private:
    void rebuild_breadcrumbs();
    void clear_breadcrumbs();

    QString m_root_path;
    QString m_current_path;
    QHBoxLayout* m_layout;
    std::vector<QPushButton*> m_buttons;
};

} // namespace editor

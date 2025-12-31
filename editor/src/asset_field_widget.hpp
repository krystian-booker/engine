#pragma once

#include "asset_item_model.hpp"
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace editor {

// Widget for displaying and selecting asset references in the Inspector
// Supports drag-and-drop from the Asset Browser
class AssetFieldWidget : public QWidget {
    Q_OBJECT

public:
    explicit AssetFieldWidget(QWidget* parent = nullptr);
    explicit AssetFieldWidget(AssetType accepted_type, QWidget* parent = nullptr);
    ~AssetFieldWidget() override;

    // Asset path
    void set_asset(const QString& path);
    QString asset() const { return m_asset_path; }

    // Accepted asset type (Unknown = accept all)
    void set_accepted_type(AssetType type);
    AssetType accepted_type() const { return m_accepted_type; }

    // Enable/disable
    void set_read_only(bool read_only);
    bool is_read_only() const { return m_read_only; }

signals:
    void asset_changed(const QString& path);
    void browse_requested();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void on_browse_clicked();
    void on_clear_clicked();

private:
    void setup_ui();
    bool accepts_drop(const QMimeData* mime_data) const;
    QString extract_asset_path(const QMimeData* mime_data) const;
    AssetType extract_asset_type(const QMimeData* mime_data) const;

    QString m_asset_path;
    AssetType m_accepted_type = AssetType::Unknown;
    bool m_read_only = false;
    bool m_drag_hover = false;

    QLabel* m_icon_label;
    QLineEdit* m_path_edit;
    QPushButton* m_browse_btn;
    QPushButton* m_clear_btn;
};

} // namespace editor

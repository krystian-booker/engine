#pragma once

#include "asset_item_model.hpp"
#include <QStyledItemDelegate>

namespace editor {

// Custom delegate for rendering asset items with thumbnails
class AssetItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit AssetItemDelegate(QObject* parent = nullptr);
    ~AssetItemDelegate() override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    // Icon size settings
    void set_icon_size(int size);
    int icon_size() const { return m_icon_size; }

    // Grid mode (icon) vs list mode
    void set_grid_mode(bool grid);
    bool grid_mode() const { return m_grid_mode; }

private:
    QIcon default_icon_for_type(AssetType type) const;
    void draw_thumbnail(QPainter* painter, const QRect& rect,
                        const QIcon& icon, ThumbnailState state) const;
    void draw_type_badge(QPainter* painter, const QRect& rect, AssetType type) const;
    void draw_favorite_star(QPainter* painter, const QRect& rect, bool is_favorite) const;

    int m_icon_size = 64;
    bool m_grid_mode = true;

    // Cached default icons
    mutable QHash<AssetType, QIcon> m_default_icons;
};

} // namespace editor

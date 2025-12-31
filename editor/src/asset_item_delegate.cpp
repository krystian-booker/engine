#include "asset_item_delegate.hpp"
#include <QPainter>
#include <QApplication>

namespace editor {

AssetItemDelegate::AssetItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

AssetItemDelegate::~AssetItemDelegate() = default;

void AssetItemDelegate::set_icon_size(int size) {
    m_icon_size = qBound(32, size, 128);
}

void AssetItemDelegate::set_grid_mode(bool grid) {
    m_grid_mode = grid;
}

void AssetItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const {
    painter->save();

    // Draw background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, option.palette.highlight().color().lighter(150));
    }

    // Get item data
    QString name = index.data(Qt::DisplayRole).toString();
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    auto type = static_cast<AssetType>(index.data(AssetItemModel::TypeRole).toInt());
    auto thumb_state = static_cast<ThumbnailState>(index.data(AssetItemModel::ThumbnailStateRole).toInt());
    bool is_favorite = index.data(AssetItemModel::IsFavoriteRole).toBool();

    // Use default icon if none provided
    if (icon.isNull()) {
        icon = default_icon_for_type(type);
    }

    if (m_grid_mode) {
        // Grid/Icon mode layout
        QRect icon_rect = option.rect;
        icon_rect.setHeight(m_icon_size);
        icon_rect.setWidth(m_icon_size);
        icon_rect.moveLeft(option.rect.left() + (option.rect.width() - m_icon_size) / 2);
        icon_rect.moveTop(option.rect.top() + 4);

        // Draw thumbnail
        draw_thumbnail(painter, icon_rect, icon, thumb_state);

        // Draw type badge
        draw_type_badge(painter, icon_rect, type);

        // Draw favorite star
        if (is_favorite) {
            draw_favorite_star(painter, icon_rect, true);
        }

        // Draw text below icon
        QRect text_rect = option.rect;
        text_rect.setTop(icon_rect.bottom() + 4);
        text_rect.adjust(2, 0, -2, -2);

        painter->setPen(option.state & QStyle::State_Selected
                       ? option.palette.highlightedText().color()
                       : option.palette.text().color());

        QFontMetrics fm(option.font);
        QString elided = fm.elidedText(name, Qt::ElideMiddle, text_rect.width());
        painter->drawText(text_rect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, elided);

    } else {
        // List mode layout
        int icon_size_small = 24;
        QRect icon_rect(option.rect.left() + 4,
                        option.rect.top() + (option.rect.height() - icon_size_small) / 2,
                        icon_size_small, icon_size_small);

        // Draw icon
        icon.paint(painter, icon_rect);

        // Draw text
        QRect text_rect = option.rect;
        text_rect.setLeft(icon_rect.right() + 8);
        text_rect.adjust(0, 0, -4, 0);

        painter->setPen(option.state & QStyle::State_Selected
                       ? option.palette.highlightedText().color()
                       : option.palette.text().color());
        painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, name);

        // Draw type label on the right
        QString type_str = AssetItemModel::type_to_string(type);
        QRect type_rect = text_rect;
        type_rect.setLeft(type_rect.right() - 80);
        painter->setPen(QColor(150, 150, 150));
        painter->drawText(type_rect, Qt::AlignVCenter | Qt::AlignRight, type_str);
    }

    painter->restore();
}

QSize AssetItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
    Q_UNUSED(option)
    Q_UNUSED(index)

    if (m_grid_mode) {
        return QSize(m_icon_size + 16, m_icon_size + 40);
    } else {
        return QSize(200, 28);
    }
}

QIcon AssetItemDelegate::default_icon_for_type(AssetType type) const {
    if (m_default_icons.contains(type)) {
        return m_default_icons[type];
    }

    // Create simple colored icons as placeholders
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor color;
    QString letter;

    switch (type) {
        case AssetType::Texture:
            color = QColor(100, 180, 100);  // Green
            letter = "T";
            break;
        case AssetType::Mesh:
            color = QColor(100, 150, 200);  // Blue
            letter = "M";
            break;
        case AssetType::Material:
            color = QColor(200, 100, 150);  // Pink
            letter = "Mt";
            break;
        case AssetType::Audio:
            color = QColor(200, 180, 100);  // Yellow
            letter = "A";
            break;
        case AssetType::Shader:
            color = QColor(150, 100, 200);  // Purple
            letter = "S";
            break;
        case AssetType::Scene:
            color = QColor(100, 200, 200);  // Cyan
            letter = "Sc";
            break;
        case AssetType::Prefab:
            color = QColor(200, 150, 100);  // Orange
            letter = "P";
            break;
        case AssetType::Folder:
            color = QColor(180, 160, 100);  // Tan
            letter = "F";
            break;
        default:
            color = QColor(128, 128, 128);  // Gray
            letter = "?";
            break;
    }

    // Draw rounded rectangle background
    painter.setBrush(color);
    painter.setPen(color.darker(120));
    painter.drawRoundedRect(4, 4, 56, 56, 8, 8);

    // Draw letter
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(20);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, letter);

    QIcon icon(pixmap);
    m_default_icons[type] = icon;
    return icon;
}

void AssetItemDelegate::draw_thumbnail(QPainter* painter, const QRect& rect,
                                        const QIcon& icon, ThumbnailState state) const {
    // Draw shadow/border
    painter->setPen(QColor(60, 60, 60));
    painter->setBrush(QColor(45, 45, 45));
    painter->drawRoundedRect(rect.adjusted(-2, -2, 2, 2), 4, 4);

    // Draw icon
    icon.paint(painter, rect);

    // Draw loading indicator if generating
    if (state == ThumbnailState::Generating) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 128));
        painter->drawRoundedRect(rect, 4, 4);

        painter->setPen(Qt::white);
        painter->drawText(rect, Qt::AlignCenter, "...");
    }
}

void AssetItemDelegate::draw_type_badge(QPainter* painter, const QRect& rect,
                                         AssetType type) const {
    if (type == AssetType::Unknown) return;

    // Small badge in bottom-right corner
    QRect badge_rect(rect.right() - 16, rect.bottom() - 16, 14, 14);

    QColor color;
    switch (type) {
        case AssetType::Texture:  color = QColor(100, 180, 100); break;
        case AssetType::Mesh:     color = QColor(100, 150, 200); break;
        case AssetType::Material: color = QColor(200, 100, 150); break;
        case AssetType::Audio:    color = QColor(200, 180, 100); break;
        case AssetType::Shader:   color = QColor(150, 100, 200); break;
        case AssetType::Scene:    color = QColor(100, 200, 200); break;
        case AssetType::Prefab:   color = QColor(200, 150, 100); break;
        default: return;
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawEllipse(badge_rect);
}

void AssetItemDelegate::draw_favorite_star(QPainter* painter, const QRect& rect,
                                            bool is_favorite) const {
    if (!is_favorite) return;

    // Star in top-right corner
    QRect star_rect(rect.right() - 14, rect.top() + 2, 12, 12);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 200, 50));  // Gold

    // Simple star shape
    QPolygonF star;
    star << QPointF(star_rect.center().x(), star_rect.top())
         << QPointF(star_rect.center().x() + 3, star_rect.center().y() - 1)
         << QPointF(star_rect.right(), star_rect.center().y() - 1)
         << QPointF(star_rect.center().x() + 4, star_rect.center().y() + 2)
         << QPointF(star_rect.center().x() + 5, star_rect.bottom())
         << QPointF(star_rect.center().x(), star_rect.center().y() + 4)
         << QPointF(star_rect.center().x() - 5, star_rect.bottom())
         << QPointF(star_rect.center().x() - 4, star_rect.center().y() + 2)
         << QPointF(star_rect.left(), star_rect.center().y() - 1)
         << QPointF(star_rect.center().x() - 3, star_rect.center().y() - 1);

    painter->drawPolygon(star);
}

} // namespace editor

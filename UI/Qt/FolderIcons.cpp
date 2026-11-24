/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Array.h>
#include <UI/Qt/FolderIcons.h>
#include <UI/Qt/Theme.h>

#include <QEventLoop>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIconEngine>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Ladybird::FolderIcons {

using FolderGlyph = TabFolderTree::FolderGlyph;

struct CatalogEntry {
    char const* id;
    char const* label;
    void (*paint)(QPainter&);
};

// The IDs, not their positions or labels, are stored in sessions. All art is drawn on a 16 × 16 canvas.
static constexpr auto CATALOG = to_array<CatalogEntry>({
    { "", "Default", nullptr },
    { "star", "Star", [](QPainter& painter) {
         painter.drawPolygon(QPolygonF { { 8, 1.8 }, { 10, 5.9 }, { 14.5, 6.5 }, { 11.2, 9.7 }, { 12, 14.2 }, { 8, 12 }, { 4, 14.2 }, { 4.8, 9.7 }, { 1.5, 6.5 }, { 6, 5.9 } });
     } },
    { "heart", "Heart", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(8, 13.8);
         path.cubicTo(5.8, 12.3, 1.7, 9.3, 1.7, 6.3);
         path.cubicTo(1.7, 2.6, 6, 1.6, 8, 4.6);
         path.cubicTo(10, 1.6, 14.3, 2.6, 14.3, 6.3);
         path.cubicTo(14.3, 9.3, 10.2, 12.3, 8, 13.8);
         path.closeSubpath();
         painter.drawPath(path);
     } },
    { "bolt", "Lightning", [](QPainter& painter) {
         painter.drawPolygon(QPolygonF { { 9.5, 1.8 }, { 3.1, 8.9 }, { 7.1, 8.9 }, { 6.5, 14.2 }, { 12.9, 7.1 }, { 8.9, 7.1 } });
     } },
    { "sun", "Sun", [](QPainter& painter) {
         painter.drawEllipse(QPointF(8, 8), 2.9, 2.9);
         painter.drawLine(QPointF(8, 1.5), QPointF(8, 2.6));
         painter.drawLine(QPointF(8, 13.4), QPointF(8, 14.5));
         painter.drawLine(QPointF(1.5, 8), QPointF(2.6, 8));
         painter.drawLine(QPointF(13.4, 8), QPointF(14.5, 8));
         painter.drawLine(QPointF(3.4, 3.4), QPointF(4.2, 4.2));
         painter.drawLine(QPointF(11.8, 11.8), QPointF(12.6, 12.6));
         painter.drawLine(QPointF(3.4, 12.6), QPointF(4.2, 11.8));
         painter.drawLine(QPointF(11.8, 4.2), QPointF(12.6, 3.4));
     } },
    { "moon", "Moon", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(8.3, 1.8);
         path.cubicTo(2.7, 1.6, .3, 8.5, 4, 12.3);
         path.cubicTo(7, 15.5, 13.1, 14, 14.1, 9.3);
         path.cubicTo(9.5, 11.7, 5.3, 6.5, 8.3, 1.8);
         path.closeSubpath();
         painter.drawPath(path);
     } },
    { "cloud", "Cloud", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(4.6, 12.3);
         path.cubicTo(.3, 12.3, .7, 6.1, 4.6, 6.6);
         path.cubicTo(5.6, 2.2, 11.8, 2.5, 12.1, 7.1);
         path.cubicTo(15.7, 7.2, 15.2, 12.3, 12.1, 12.3);
         path.closeSubpath();
         painter.drawPath(path);
     } },
    { "leaf", "Leaf", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(4.1, 11.9);
         path.cubicTo(.3, 6.2, 6.9, 2.9, 13.6, 2.3);
         path.cubicTo(13, 9.5, 9.6, 15, 4.1, 11.9);
         path.closeSubpath();
         painter.drawPath(path);
         painter.drawLine(QPointF(2.5, 13.7), QPointF(9.7, 6.5));
     } },
    { "mountains", "Mountains", [](QPainter& painter) {
         painter.drawPolygon(QPolygonF { { 1.7, 13 }, { 6.1, 4.2 }, { 9.2, 9.5 }, { 11.2, 6.6 }, { 14.3, 13 } });
         painter.drawPolyline(QPolygonF { { 4.5, 7.6 }, { 6.1, 8.5 }, { 7.7, 7.6 } });
     } },
    { "waves", "Waves", [](QPainter& painter) {
         QPainterPath path;
         for (auto y : { 4, 8, 12 }) {
             path.moveTo(1.8, y);
             path.cubicTo(3.8, y - 2.3, 5.6, y - 2.3, 8, y);
             path.cubicTo(10.4, y + 2.3, 12.2, y + 2.3, 14.2, y);
         }
         painter.drawPath(path);
     } },
    { "globe", "Globe", [](QPainter& painter) {
         painter.drawEllipse(QPointF(8, 8), 6, 6);
         painter.drawEllipse(QPointF(8, 8), 2.6, 6);
         painter.drawLine(QPointF(2, 8), QPointF(14, 8));
     } },
    { "flag", "Flag", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(3, 14.2);
         path.lineTo(3, 3.3);
         path.cubicTo(7, .2, 10, 5.8, 13.2, 2.7);
         path.lineTo(13.2, 9);
         path.cubicTo(10, 12.1, 7, 6.5, 3, 9.6);
         painter.drawPath(path);
     } },
    { "compass", "Compass", [](QPainter& painter) {
         painter.drawEllipse(QPointF(8, 8), 6, 6);
         painter.drawPolygon(QPolygonF { { 11.1, 4.9 }, { 9.2, 9.2 }, { 4.9, 11.1 }, { 6.8, 6.8 } });
     } },
    { "home", "Home", [](QPainter& painter) {
         painter.drawPolygon(QPolygonF { { 2.6, 7.2 }, { 8, 2.4 }, { 13.4, 7.2 }, { 13.4, 13.6 }, { 2.6, 13.6 } });
         painter.drawPolyline(QPolygonF { { 6.3, 13.6 }, { 6.3, 9.2 }, { 9.7, 9.2 }, { 9.7, 13.6 } });
     } },
    { "briefcase", "Briefcase", [](QPainter& painter) {
         painter.drawRoundedRect(QRectF(1.8, 5, 12.4, 8.4), 1.4, 1.4);
         painter.drawPolyline(QPolygonF { { 5.4, 5 }, { 5.4, 2.6 }, { 10.6, 2.6 }, { 10.6, 5 } });
         painter.drawLine(QPointF(2, 8.5), QPointF(14, 8.5));
         painter.drawLine(QPointF(7, 9.8), QPointF(9, 9.8));
     } },
    { "book", "Book", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(8, 4);
         path.quadTo(5.1, 1.5, 1.8, 3.4);
         path.lineTo(1.8, 12.3);
         path.quadTo(5.1, 10.7, 8, 13);
         path.quadTo(10.9, 10.7, 14.2, 12.3);
         path.lineTo(14.2, 3.4);
         path.quadTo(10.9, 1.5, 8, 4);
         path.closeSubpath();
         painter.drawPath(path);
         painter.drawLine(QPointF(8, 4), QPointF(8, 13));
     } },
    { "note", "Note", [](QPainter& painter) {
         painter.drawPolygon(QPolygonF { { 3.6, 1.8 }, { 9.1, 1.8 }, { 12.4, 5.1 }, { 12.4, 14.2 }, { 3.6, 14.2 } });
         painter.drawPolyline(QPolygonF { { 9.1, 1.8 }, { 9.1, 5.1 }, { 12.4, 5.1 } });
         painter.drawLine(QPointF(5.9, 8.2), QPointF(10.1, 8.2));
         painter.drawLine(QPointF(5.9, 11), QPointF(9.1, 11));
     } },
    { "calendar", "Calendar", [](QPainter& painter) {
         painter.drawRoundedRect(QRectF(2.1, 3.4, 11.8, 10.5), 1.3, 1.3);
         painter.drawLine(QPointF(2.1, 6.7), QPointF(13.9, 6.7));
         painter.drawLine(QPointF(5.1, 1.8), QPointF(5.1, 4.8));
         painter.drawLine(QPointF(10.9, 1.8), QPointF(10.9, 4.8));
         painter.drawLine(QPointF(6.8, 10.4), QPointF(9.2, 10.4));
     } },
    { "code", "Code", [](QPainter& painter) {
         painter.drawPolyline(QPolygonF { { 4.6, 4.8 }, { 1.8, 8 }, { 4.6, 11.2 } });
         painter.drawLine(QPointF(9.5, 3), QPointF(6.5, 13));
         painter.drawPolyline(QPolygonF { { 11.4, 4.8 }, { 14.2, 8 }, { 11.4, 11.2 } });
     } },
    { "flask", "Flask", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(6.1, 2.1);
         path.lineTo(6.1, 6.5);
         path.lineTo(2.8, 11.8);
         path.quadTo(1.6, 13.9, 4.1, 13.9);
         path.lineTo(11.9, 13.9);
         path.quadTo(14.4, 13.9, 13.2, 11.8);
         path.lineTo(9.9, 6.5);
         path.lineTo(9.9, 2.1);
         painter.drawPath(path);
         painter.drawLine(QPointF(5.1, 2.1), QPointF(10.9, 2.1));
         painter.drawLine(QPointF(4.5, 9.3), QPointF(11.5, 9.3));
     } },
    { "music", "Music", [](QPainter& painter) {
         painter.drawEllipse(QRectF(2.2, 10.7, 4.2, 3.3));
         painter.drawEllipse(QRectF(9.6, 9, 4.2, 3.3));
         painter.drawPolyline(QPolygonF { { 6.4, 12.4 }, { 6.4, 3.8 }, { 13.8, 2.1 }, { 13.8, 10.7 } });
         painter.drawLine(QPointF(6.4, 6.3), QPointF(13.8, 4.6));
     } },
    { "camera", "Camera", [](QPainter& painter) {
         painter.drawPolygon(QPolygonF { { 1.7, 5 }, { 4.8, 5 }, { 6, 3 }, { 10, 3 }, { 11.2, 5 }, { 14.3, 5 }, { 14.3, 13 }, { 1.7, 13 } });
         painter.drawEllipse(QPointF(8, 8.8), 2.4, 2.4);
     } },
    { "film", "Film", [](QPainter& painter) {
         painter.drawRoundedRect(QRectF(2.1, 2.6, 11.8, 10.8), 1.2, 1.2);
         painter.drawLine(QPointF(5.3, 2.6), QPointF(5.3, 13.4));
         painter.drawLine(QPointF(10.7, 2.6), QPointF(10.7, 13.4));
         painter.drawLine(QPointF(2.1, 8), QPointF(5.3, 8));
         painter.drawLine(QPointF(10.7, 8), QPointF(13.9, 8));
     } },
    { "gamepad", "Gamepad", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(5.3, 4.2);
         path.lineTo(10.7, 4.2);
         path.cubicTo(13, 4.2, 13.9, 6.1, 14.4, 10.7);
         path.cubicTo(14.7, 13.5, 12, 13.5, 10.2, 10.9);
         path.lineTo(5.8, 10.9);
         path.cubicTo(4, 13.5, 1.3, 13.5, 1.6, 10.7);
         path.cubicTo(2.1, 6.1, 3, 4.2, 5.3, 4.2);
         path.closeSubpath();
         painter.drawPath(path);
         painter.drawLine(QPointF(3.7, 8), QPointF(6.5, 8));
         painter.drawLine(QPointF(5.1, 6.6), QPointF(5.1, 9.4));
         painter.drawPoint(QPointF(10, 8.8));
         painter.drawPoint(QPointF(11.7, 7.1));
     } },
    { "coffee", "Coffee", [](QPainter& painter) {
         QPainterPath path;
         path.moveTo(2.3, 5.4);
         path.lineTo(11.2, 5.4);
         path.lineTo(11.2, 8.3);
         path.quadTo(11.2, 12.4, 6.8, 12.4);
         path.quadTo(2.3, 12.4, 2.3, 8.3);
         path.closeSubpath();
         path.moveTo(11.2, 6);
         path.cubicTo(15.5, 5.4, 15.5, 10, 11, 9.9);
         painter.drawPath(path);
         painter.drawLine(QPointF(1.8, 14.1), QPointF(12.4, 14.1));
         painter.drawLine(QPointF(5.2, 2.3), QPointF(5.2, 3.2));
         painter.drawLine(QPointF(8.3, 1.8), QPointF(8.3, 3.2));
     } },
});

static CatalogEntry const& find_entry(QString const& id)
{
    for (auto const& entry : CATALOG) {
        if (id == QLatin1StringView(entry.id))
            return entry;
    }
    return CATALOG[0];
}

static void paint_default_folder(QPainter& painter, FolderGlyph glyph)
{
    QPainterPath outline;
    if (glyph == FolderGlyph::Open) {
        // The back of the folder meets the sloped front flap at its top edge.
        outline.moveTo(11.9, 7.1);
        outline.lineTo(11.9, 5.9);
        outline.quadTo(11.9, 4.8, 10.8, 4.8);
        outline.lineTo(7.2, 4.8);
        outline.lineTo(5.8, 3.2);
        outline.lineTo(2.6, 3.2);
        outline.quadTo(1.5, 3.2, 1.5, 4.3);
        outline.lineTo(1.5, 12.1);
        outline.quadTo(1.5, 13.2, 2.6, 13.2);
        outline.lineTo(11.6, 13.2);
        outline.quadTo(12.4, 13.2, 12.7, 12.4);
        outline.lineTo(14.5, 7.9);
        outline.quadTo(14.8, 7.1, 13.9, 7.1);
        outline.lineTo(4.5, 7.1);
        outline.lineTo(2.3, 13);
    } else {
        // Leave space above the closed folder's lip for the active tab to peek out.
        auto lip = glyph == FolderGlyph::ClosedWithActiveTab ? 6.4 : 4.8;
        if (glyph == FolderGlyph::ClosedWithActiveTab) {
            outline.moveTo(8.1, lip);
            outline.lineTo(8.1, 3.9);
            outline.quadTo(8.1, 3.1, 8.9, 3.1);
            outline.lineTo(11.7, 3.1);
            outline.quadTo(12.5, 3.1, 12.5, 3.9);
            outline.lineTo(12.5, lip);
        }
        outline.moveTo(1.5, 4.3);
        outline.quadTo(1.5, 3.2, 2.6, 3.2);
        outline.lineTo(5.8, 3.2);
        outline.lineTo(7.2, lip);
        outline.lineTo(13.4, lip);
        outline.quadTo(14.5, lip, 14.5, lip + 1.1);
        outline.lineTo(14.5, 12.1);
        outline.quadTo(14.5, 13.2, 13.4, 13.2);
        outline.lineTo(2.6, 13.2);
        outline.quadTo(1.5, 13.2, 1.5, 12.1);
        outline.closeSubpath();
    }
    painter.drawPath(outline);
}

static void paint_icon(QPainter& painter, QRectF const& rect, CatalogEntry const& entry, FolderGlyph glyph, QColor const& color)
{
    auto side = min(rect.width(), rect.height());
    if (side <= 0)
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.center() - QPointF(side / 2, side / 2));
    painter.scale(side / 16, side / 16);
    painter.setPen(QPen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (!entry.paint) {
        paint_default_folder(painter, glyph);
    } else if (glyph == FolderGlyph::ClosedWithActiveTab) {
        // Preserve the custom symbol and leave a clear gap around its active-descendant dot.
        painter.save();
        painter.translate(.45, .45);
        painter.scale(.76, .76);
        entry.paint(painter);
        painter.restore();
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(13.5, 13.5), 1.65, 1.65);
    } else {
        entry.paint(painter);
    }

    painter.restore();
}

class FolderIconEngine final : public QIconEngine {
public:
    AK_ALLOC_WITH_KMALLOC;

    FolderIconEngine(CatalogEntry const& entry, FolderGlyph glyph, QColor const& color)
        : m_entry(entry)
        , m_glyph(glyph)
        , m_color(color)
    {
    }

    virtual QIconEngine* clone() const override { return new FolderIconEngine(m_entry, m_glyph, m_color); }

    virtual QSize actualSize(QSize const& size, QIcon::Mode, QIcon::State) override
    {
        auto side = min(size.width(), size.height());
        return { side, side };
    }

    virtual void paint(QPainter* painter, QRect const& rect, QIcon::Mode, QIcon::State) override
    {
        paint_icon(*painter, rect, m_entry, m_glyph, m_color);
    }

    virtual QPixmap pixmap(QSize const& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1);
    }

    virtual QPixmap scaledPixmap(QSize const& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        if (size.isEmpty())
            return {};
        QPixmap pixmap(size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);
        {
            QPainter painter(&pixmap);
            paint(&painter, QRect(QPoint(), size), mode, state);
        }
        return pixmap;
    }

private:
    CatalogEntry const& m_entry;
    FolderGlyph m_glyph;
    QColor m_color;
};

QIcon icon(QString const& id, FolderGlyph glyph, QColor const& color)
{
    return QIcon(new FolderIconEngine(find_entry(id), glyph, color));
}

class FolderIconDelegate final : public QStyledItemDelegate {
public:
    AK_ALLOC_WITH_KMALLOC;

    using QStyledItemDelegate::QStyledItemDelegate;

    virtual void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override
    {
        auto const& entry = find_entry(index.data(Qt::UserRole).toString());
        auto const& panel = Theme::panel();
        auto selected = option.state.testFlag(QStyle::State_Selected);
        auto hovered = option.state.testFlag(QStyle::State_MouseOver);
        if (selected || hovered) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(panel.control);
            painter->drawRoundedRect(QRectF(option.rect).adjusted(2, 2, -2, -2), 6, 6);
            painter->restore();
        }

        // Render only the glyph. The model keeps its label for accessibility.
        QRect glyph_rect(0, 0, 18, 18);
        glyph_rect.moveCenter(option.rect.center());
        paint_icon(*painter, glyph_rect, entry, FolderGlyph::Closed, panel.text);
    }
};

class FolderIconPopup final : public QWidget {
public:
    AK_ALLOC_WITH_KMALLOC;

    FolderIconPopup(QWidget* parent, QString const& current, QEventLoop& loop)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
        , m_loop(loop)
    {
        setAttribute(Qt::WA_TranslucentBackground);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(4);

        auto* heading = new QLabel("Icons", this);
        heading->setIndent(4);
        auto heading_font = heading->font();
        heading_font.setWeight(QFont::DemiBold);
        heading->setFont(heading_font);
        auto heading_palette = heading->palette();
        heading_palette.setColor(QPalette::WindowText, Theme::panel().text);
        heading->setPalette(heading_palette);
        layout->addWidget(heading);

        constexpr int columns = 5;
        constexpr int rows = (CATALOG.size() + columns - 1) / columns;
        constexpr int tile_size = 32;
        m_grid = new QTableWidget(rows, columns, this);
        m_grid->setAccessibleName("Folder icons");
        m_grid->setItemDelegate(new FolderIconDelegate(m_grid));
        m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
        m_grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_grid->setTabKeyNavigation(false);
        m_grid->setMouseTracking(true);
        m_grid->setFrameShape(QFrame::NoFrame);
        m_grid->setShowGrid(false);
        m_grid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_grid->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_grid->viewport()->setAutoFillBackground(false);
        for (auto* header : { m_grid->horizontalHeader(), m_grid->verticalHeader() }) {
            header->hide();
            header->setSectionResizeMode(QHeaderView::Fixed);
            header->setMinimumSectionSize(tile_size);
            header->setDefaultSectionSize(tile_size);
        }
        m_grid->setFixedSize(columns * tile_size, rows * tile_size);
        layout->addWidget(m_grid);

        auto const& initial = find_entry(current);
        for (size_t i = 0; i < CATALOG.size(); ++i) {
            auto const& entry = CATALOG[i];
            auto* item = new QTableWidgetItem(QLatin1StringView(entry.label));
            item->setData(Qt::UserRole, QString::fromLatin1(entry.id));
            item->setData(Qt::AccessibleTextRole, item->text());
            m_grid->setItem(i / columns, i % columns, item);
            if (&entry == &initial)
                m_grid->setCurrentItem(item);
        }

        connect(m_grid, &QTableWidget::itemClicked, this, &FolderIconPopup::commit);
        m_grid->installEventFilter(this);
        setFocusProxy(m_grid);
        setFocus(Qt::PopupFocusReason);
        setFixedSize(sizeHint());
    }

    std::optional<QString> choice() const { return m_choice; }

    void move_below(QRect const& anchor_global)
    {
        constexpr int gap = 4;
        auto x = layoutDirection() == Qt::RightToLeft ? anchor_global.right() - width() + 1 : anchor_global.left();
        auto y = anchor_global.bottom() + 1 + gap;
        auto* anchor_screen = QGuiApplication::screenAt(anchor_global.center());
        if (!anchor_screen)
            anchor_screen = parentWidget() ? parentWidget()->screen() : screen();
        if (anchor_screen) {
            auto bounds = anchor_screen->availableGeometry();
            if (y + height() > bounds.bottom() + 1)
                y = anchor_global.top() - gap - height();
            x = clamp(x, bounds.left(), max(bounds.left(), bounds.right() - width() + 1));
            y = clamp(y, bounds.top(), max(bounds.top(), bounds.bottom() - height() + 1));
        }
        move(x, y);
    }

private:
    void commit(QTableWidgetItem* item)
    {
        if (!item)
            return;
        m_choice = item->data(Qt::UserRole).toString();
        close();
    }

    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_grid && event->type() == QEvent::KeyPress) {
            switch (static_cast<QKeyEvent*>(event)->key()) {
            case Qt::Key_Escape:
                close();
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Space:
                commit(m_grid->currentItem());
                return true;
            default:
                break;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    virtual void hideEvent(QHideEvent* event) override
    {
        QWidget::hideEvent(event);
        m_loop.quit();
    }

    virtual void paintEvent(QPaintEvent*) override
    {
        auto const& panel = Theme::panel();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(panel.edge);
        painter.setBrush(panel.surface);
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 10, 10);
    }

    QEventLoop& m_loop;
    QTableWidget* m_grid { nullptr };
    std::optional<QString> m_choice;
};

std::optional<QString> choose(QWidget* parent, QString const& current, QRect const& anchor_global)
{
    // Wait for a choice without making the window modal. Qt handles outside
    // clicks as popup dismissals, including compositor-driven hides.
    QEventLoop loop;
    QPointer popup = new FolderIconPopup(parent, current, loop);
    QObject::connect(popup, &QObject::destroyed, &loop, &QEventLoop::quit);
    popup->move_below(anchor_global);
    popup->show();
    if (popup && popup->isVisible())
        loop.exec();
    if (!popup)
        return {};
    auto choice = popup->choice();
    delete popup.data();
    return choice;
}

}

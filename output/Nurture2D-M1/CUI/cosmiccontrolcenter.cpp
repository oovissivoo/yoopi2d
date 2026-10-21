#include "cosmiccontrolcenter.h"
#include "cosmicuimetrics.h"
#include "cosmicpopupcontextmenu.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QSettings>
#include <QAction>
#include <QApplication>
#include <QResizeEvent>
#include <QEvent>
#include <QToolTip>

static constexpr char kSettingsPrefix[] = "CosmicControlCenter";

// ── geometry ──────────────────────────────────────────────────────────────────

int CosmicControlCenter::barHeight() const
{
    return CosmicUiMetrics::controlCenterThickness() + 8;
}

int CosmicControlCenter::cellWidth() const
{
    const int w = width();
    if (w > 0) {
        // Always divide by total cells to ensure all slots fit within available width
        // No minimum constraint - let it scale down as needed
        return qMax(12, w / CosmicControlCenterLayout::kCells); // 12px absolute minimum
    }
    return CosmicUiMetrics::controlCenterThickness() + 6;
}

int CosmicControlCenter::cellHeight() const
{
    return CosmicUiMetrics::controlCenterThickness();
}

int CosmicControlCenter::cellAt(int x) const
{
    return qBound(0, x / cellWidth(), CosmicControlCenterLayout::kCells - 1);
}

QRect CosmicControlCenter::cellRect(int cell) const
{
    const int cw = cellWidth();
    const int ch = cellHeight();
    const int y  = (barHeight() - ch) / 2;
    return QRect(cell * cw, y, cw, ch);
}

QRect CosmicControlCenter::groupRect(int anchorCell, int span) const
{
    QRect r = cellRect(anchorCell);
    r.setWidth(cellWidth() * span);
    return r;
}

QSize CosmicControlCenter::sizeHint() const
{
    return QSize((CosmicUiMetrics::controlCenterThickness() + 6) * CosmicControlCenterLayout::kCells,
                 barHeight());
}

// ── constructor ───────────────────────────────────────────────────────────────

CosmicControlCenter::CosmicControlCenter(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(barHeight());
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void CosmicControlCenter::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    repositionHostedWidgets();
    update();
}

// ── context menu ──────────────────────────────────────────────────────────────

void CosmicControlCenter::contextMenuEvent(QContextMenuEvent* e)
{
    showContextMenu(e->globalPos());
}

void CosmicControlCenter::showContextMenu(const QPoint& globalPos)
{
    CosmicPopupContextMenu menu(this);
    menu.addCheckableItem(tr("Edit Layout"), QStringLiteral("edit"), mEditMode);
    const QString chosen = menu.execAt(globalPos);
    if (chosen == QLatin1String("edit"))
        toggleEditMode();
}

void CosmicControlCenter::toggleEditMode()
{
    mEditMode = !mEditMode;
    if (!mEditMode) {
        mDragging = false;
        for (QWidget* w : mHostedWidgets)
            if (w) w->show();
        saveLayout();
    }
    for (QWidget* w : mHostedWidgets) {
        if (!w) continue;
        w->setAttribute(Qt::WA_TransparentForMouseEvents, mEditMode);
        for (QWidget* c : w->findChildren<QWidget*>())
            c->setAttribute(Qt::WA_TransparentForMouseEvents, mEditMode);
    }
    update();
    emit editModeChanged(mEditMode);
}

bool CosmicControlCenter::eventFilter(QObject* obj, QEvent* event)
{
    Q_UNUSED(obj) Q_UNUSED(event)
    return false;
}

// ── item registration ─────────────────────────────────────────────────────────

void CosmicControlCenter::addSingle(const QString& id, QAction* action)
{
    mLayout.addSingle(id, action);
    if (action)
        connect(action, &QAction::changed, this, QOverload<>::of(&QWidget::update));
    update();
}

void CosmicControlCenter::addGroup(const QString& groupId, const QVector<QAction*>& actions)
{
    mLayout.addGroup(groupId, actions);
    for (QAction* a : actions)
        if (a) connect(a, &QAction::changed, this, QOverload<>::of(&QWidget::update));
    update();
}

void CosmicControlCenter::addWidgetGroup(const QString& groupId, QWidget* widget, int span)
{
    mLayout.addGroup(groupId, QVector<QAction*>(span, nullptr));
    widget->setParent(this);
    widget->show();
    mHostedWidgets.insert(groupId, widget);
    repositionHostedWidgets();
    update();
}

void CosmicControlCenter::addSpacer()
{
    mLayout.addEmpty(-1);
    update();
}

// ── hosted widget positioning ─────────────────────────────────────────────────

void CosmicControlCenter::repositionHostedWidgets()
{
    for (auto it = mHostedWidgets.constBegin(); it != mHostedWidgets.constEnd(); ++it) {
        QWidget* w = it.value();
        if (!w) continue;
        int anchor = CosmicControlCenterLayout::kCells;
        int span   = 0;
        for (const auto& item : mLayout.items()) {
            if (item.groupId == it.key() && item.isGroup() && item.cell >= 0) {
                anchor = qMin(anchor, item.cell);
                ++span;
            }
        }
        if (span == 0) continue;
        QRect r = groupRect(anchor, span);
        r.adjust(1, 1, -1, -1);
        w->setGeometry(r);
        w->raise();
    }
}

// ── persistence ───────────────────────────────────────────────────────────────

void CosmicControlCenter::saveLayout()
{
    QSettings s;
    s.beginGroup(QLatin1String(kSettingsPrefix));
    // Persist which groups are currently hidden
    QStringList hidden(mHiddenGroups.values());
    hidden.sort();
    s.setValue(QStringLiteral("hiddenGroups"), hidden);
    s.endGroup();
    mLayout.save(s, QLatin1String(kSettingsPrefix));
}

void CosmicControlCenter::loadLayout()
{
    QSettings s;
    // Restore hidden-group set before loading layout so paint is correct
    s.beginGroup(QLatin1String(kSettingsPrefix));
    const QStringList hidden = s.value(QStringLiteral("hiddenGroups")).toStringList();
    s.endGroup();
    mHiddenGroups = QSet<QString>(hidden.begin(), hidden.end());

    mLayout.load(s, QLatin1String(kSettingsPrefix));
    // Re-hide hosted widgets that were hidden at last save
    for (const QString& gid : qAsConst(mHiddenGroups)) {
        QWidget* hw = mHostedWidgets.value(gid, nullptr);
        if (hw) hw->setVisible(false);
    }
    repositionHostedWidgets();
    update();
}

void CosmicControlCenter::setGroupVisible(const QString& groupId, bool visible)
{
    if (visible) {
        mHiddenGroups.remove(groupId);
        mLayout.showByKey(groupId);
        // Restore hosted widget visibility
        QWidget* hw = mHostedWidgets.value(groupId, nullptr);
        if (hw) hw->setVisible(true);
        repositionHostedWidgets();
    } else {
        mHiddenGroups.insert(groupId);
        mLayout.hideByKey(groupId);
        // Hide hosted widget
        QWidget* hw = mHostedWidgets.value(groupId, nullptr);
        if (hw) hw->setVisible(false);
    }
    saveLayout(); // persist both hide and show immediately
    update();
}

bool CosmicControlCenter::isGroupVisible(const QString& groupId) const
{
    return !mHiddenGroups.contains(groupId);
}

// ── painting helpers ──────────────────────────────────────────────────────────

static void paintTintedIcon(QPainter& p, const QIcon& icon, const QRect& rect,
                            const QColor& color, QIcon::Mode mode = QIcon::Normal)
{
    const qreal dpr  = p.device() ? p.device()->devicePixelRatioF() : 1.0;
    QPixmap tinted(qRound(rect.width() * dpr), qRound(rect.height() * dpr));
    tinted.setDevicePixelRatio(dpr);
    tinted.fill(Qt::transparent);

    QPainter iconPainter(&tinted);
    icon.paint(&iconPainter, QRect(QPoint(0, 0), rect.size()), Qt::AlignCenter, mode);
    iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    iconPainter.fillRect(QRect(QPoint(0, 0), rect.size()), color);
    iconPainter.end();

    // Let Qt map the device-pixel-ratio-aware pixmap to its logical position.
    p.drawPixmap(rect.topLeft(), tinted);
}

// ── paintEvent ────────────────────────────────────────────────────────────────

void CosmicControlCenter::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), CosmicColor::menuBarBackground());
    p.setPen(QColor(0x18, 0x18, 0x18));
    p.drawLine(0, 0, width(), 0);

    const int cw = cellWidth();
    const int outerPad = qMax(1, cw / 10);
    const int radius = qMax(2, qMin(CosmicUiMetrics::cornerRadius(), cw / 6));

    for (int c = 0; c < CosmicControlCenterLayout::kCells; ++c) {
        // During drag: skip the dragged cells but still draw empty placeholder
        if (mDragging && c >= mDragAnchorCell && c < mDragAnchorCell + mDragSpan) {
            // Draw dashed empty outline so the vacated space is visible
            if (mEditMode) {
                const QRect r = cellRect(c);
                p.setPen(QPen(QColor(0x3a, 0x3a, 0x3a), 1, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(r.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);
            }
            continue;
        }
        drawCell(p, c);
    }

    if (mDragging)
        drawGhost(p);
}

void CosmicControlCenter::drawCell(QPainter& p, int cell)
{
    const CosmicControlCenterItem item = mLayout.itemAt(cell);
    const QRect  r      = cellRect(cell);
    const int    cw     = cellWidth();
    const int    radius = qMax(2, qMin(CosmicUiMetrics::cornerRadius(), cw / 6)); // Scale radius
    const bool   edit   = mEditMode;
    
    // Adaptive padding based on cell width
    const int outerPad = qMax(1, cw / 10);  // Outer padding scales with cell size
    const int innerPad = qMax(1, cw / 8);   // Inner padding for icons

    if (!item.isValid() || item.isEmpty()) {
        if (edit) {
            p.setPen(QPen(QColor(0x3a, 0x3a, 0x3a), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);
        }
        return;
    }

    if (item.isGroup()) {
        int start = mLayout.groupStart(cell);
        if (start < 0) start = cell;
        if (start != cell) return;
        int span = mLayout.groupSpan(cell);
        if (span < 1) span = 1;
        QRect gr = groupRect(start, span);

        // Skip hidden groups — draw as empty space
        if (mHiddenGroups.contains(item.groupId)) {
            if (edit) {
                p.setPen(QPen(QColor(0x3a, 0x3a, 0x3a), 1, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(gr.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);
            }
            return;
        }

        const bool hover = edit && (mHoverCell >= start && mHoverCell < start + span);
        p.setPen(Qt::NoPen);
        p.setBrush(hover ? CosmicColor::menuBarHover() : QColor(0x33, 0x33, 0x33));
        p.drawRoundedRect(gr.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);

        if (mHostedWidgets.contains(item.groupId)) {
            if (edit) {
                p.setPen(QPen(CosmicColor::accent(), 1));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(gr.adjusted(outerPad/2, outerPad/2, -outerPad/2, -outerPad/2), radius, radius);
            }
            return;
        }

        for (int i = 0; i < span; ++i) {
            CosmicControlCenterItem sub = mLayout.itemAt(start + i);
            if (!sub.isValid() || !sub.action) continue;
            const bool subChecked = sub.action->isChecked();
            if (subChecked) {
                p.setPen(Qt::NoPen);
                p.setBrush(CosmicColor::accent());
                p.drawRoundedRect(QRect(gr.x() + i*cw + outerPad, gr.y() + outerPad, 
                                       cw - outerPad*2, gr.height() - outerPad*2),
                                  radius, radius);
            }
            QIcon icon = sub.action->icon();
            if (icon.isNull()) continue;
            paintTintedIcon(p, icon,
                            QRect(gr.x() + i*cw + innerPad, gr.y() + innerPad, 
                                 cw - innerPad*2, gr.height() - innerPad*2),
                            subChecked ? CosmicColor::menuBarBackground() : QColor(0xb3, 0xb3, 0xb3),
                            sub.action->isEnabled() ? QIcon::Normal : QIcon::Disabled);
        }
        if (edit) {
            p.setPen(QPen(CosmicColor::accent(), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(gr.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);
        }
        return;
    }

    // single
    const bool checked = item.action && item.action->isChecked();
    const bool hover   = edit && (mHoverCell == cell);
    p.setPen(Qt::NoPen);
    p.setBrush(checked ? CosmicColor::accent()
                       : hover ? CosmicColor::menuBarHover() : QColor(0x33, 0x33, 0x33));
    p.drawRoundedRect(r.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);

    if (item.action) {
        QIcon icon = item.action->icon();
        if (!icon.isNull()) {
            paintTintedIcon(p, icon, r.adjusted(innerPad, innerPad, -innerPad, -innerPad),
                            checked ? CosmicColor::menuBarBackground() : QColor(0xb3, 0xb3, 0xb3),
                            item.action->isEnabled() ? QIcon::Normal : QIcon::Disabled);
        } else {
            p.setPen(checked ? CosmicColor::menuBarBackground() : CosmicColor::menuBarLabel());
            // Scale font size based on cell width
            QFont f = p.font(); 
            f.setPointSize(qMax(5, qMin(7, cw / 5))); 
            p.setFont(f);
            p.drawText(r, Qt::AlignCenter, item.action->text().left(4));
        }
    }
    if (edit) {
        p.setPen(QPen(CosmicColor::accent().darker(130), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);
    }
}

void CosmicControlCenter::drawGhost(QPainter& p)
{
    const int cw     = cellWidth();
    const int radius = qMax(2, qMin(CosmicUiMetrics::cornerRadius(), cw / 6));
    const int outerPad = qMax(1, cw / 10);
    const int innerPad = qMax(1, cw / 8);

    int ghostLeft = mDragPos.x() - mDragGrabOffset;
    int snapCell  = qBound(0, ghostLeft / cw, CosmicControlCenterLayout::kCells - mDragSpan);
    QRect ghostRect = groupRect(snapCell, mDragSpan);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x44, 0x44, 0x44, 210));
    p.drawRoundedRect(ghostRect.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);

    CosmicControlCenterItem anchorItem = mLayout.itemAt(mDragAnchorCell);
    if (anchorItem.isGroup() && mHostedWidgets.contains(anchorItem.groupId)) {
        QWidget* hw = mHostedWidgets.value(anchorItem.groupId);
        if (hw) {
            p.setOpacity(0.75);
            p.drawPixmap(ghostRect.adjusted(outerPad, outerPad, -outerPad, -outerPad), hw->grab());
            p.setOpacity(1.0);
        }
    } else {
        p.setOpacity(0.75);
        for (int i = 0; i < mDragSpan; ++i) {
            CosmicControlCenterItem item = mLayout.itemAt(mDragAnchorCell + i);
            if (!item.isValid() || !item.action) continue;
            QIcon icon = item.action->icon();
            if (icon.isNull()) continue;
            paintTintedIcon(p, icon,
                            QRect(ghostRect.x() + i*cw + innerPad, ghostRect.y() + innerPad,
                                  cw - innerPad*2, ghostRect.height() - innerPad*2),
                            QColor(0xb3, 0xb3, 0xb3));
        }
        p.setOpacity(1.0);
    }

    p.setPen(QPen(CosmicColor::accent(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(ghostRect.adjusted(outerPad, outerPad, -outerPad, -outerPad), radius, radius);
}

// ── mouse ─────────────────────────────────────────────────────────────────────

void CosmicControlCenter::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) return;

    int cell = cellAt(event->x());
    CosmicControlCenterItem item = mLayout.itemAt(cell);

    if (!mEditMode) {
        if (item.isValid() && !item.isEmpty() && item.action && item.action->isEnabled())
            item.action->trigger();
        return;
    }

    if (event->button() != Qt::LeftButton) return;
    if (!item.isValid() || item.isEmpty()) return;

    int anchor = cell, span = 1;
    if (item.isGroup()) {
        int gs = mLayout.groupStart(cell);
        anchor = (gs >= 0) ? gs : cell;
        span   = qMax(1, mLayout.groupSpan(cell));
    }

    mDragging       = true;
    mDragAnchorCell = anchor;
    mDragSpan       = span;
    mDragGrabOffset = qBound(0, event->x() - cellRect(anchor).x(), span * cellWidth() - 1);
    mDragPos        = event->pos();

    if (item.isGroup()) {
        QWidget* hw = mHostedWidgets.value(item.groupId, nullptr);
        if (hw) hw->hide();
    }
    update();
}

void CosmicControlCenter::mouseMoveEvent(QMouseEvent* event)
{
    const int prevHover = mHoverCell;
    mHoverCell = cellAt(event->x());

    if (mEditMode && mDragging)
        mDragPos = event->pos();

    // Show tooltip for the action under the cursor (not during drag)
    if (!mDragging) {
        CosmicControlCenterItem item = mLayout.itemAt(mHoverCell);
        QString tip;
        if (item.isValid() && !item.isEmpty() && item.action) {
            tip = item.action->toolTip();
            // QAction::toolTip() returns text() if no explicit tooltip was set,
            // but only when the action is in a widget. When called directly it
            // may return empty — fall back to text() explicitly.
            if (tip.isEmpty())
                tip = item.action->text().remove(QLatin1Char('&'));
        }
        if (tip.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(event->globalPos(), tip, this, cellRect(mHoverCell));
    }

    if (mHoverCell != prevHover)
        update();
    else if (mDragging)
        update();
}

void CosmicControlCenter::mouseReleaseEvent(QMouseEvent* event)
{
    if (!mEditMode || !mDragging) return;
    if (event->button() != Qt::LeftButton) return;

    const int cw = cellWidth();
    int snapCell = qBound(0, (event->x() - mDragGrabOffset) / cw,
                          CosmicControlCenterLayout::kCells - mDragSpan);

    if (mLayout.moveItem(mDragAnchorCell, snapCell))
        repositionHostedWidgets();

    for (auto it = mHostedWidgets.constBegin(); it != mHostedWidgets.constEnd(); ++it)
        if (it.value()) it.value()->show();

    mDragging       = false;
    mDragAnchorCell = -1;
    update();
}

void CosmicControlCenter::leaveEvent(QEvent*)
{
    mHoverCell = -1;
    QToolTip::hideText();
    update();
}

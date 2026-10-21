#include "cosmicmenuwindow.h"
#include "cosmicmenubar.h"
#include "cosmicuimetrics.h"

// Out-of-line definitions required for ODR-use in C++11/14
constexpr int CosmicMenuWindow::kMinWidth;
constexpr int CosmicMenuWindow::kToggleAreaWidth;
constexpr int CosmicMenuWindow::kTextLeftPad;
constexpr int CosmicMenuWindow::kTextRightPad;
constexpr int CosmicMenuWindow::kShortcutGap;
constexpr int CosmicMenuWindow::kRowMinHeight;
constexpr int CosmicMenuWindow::kSeparatorVMargin;
constexpr int CosmicMenuWindow::kWindowVPad;

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QHideEvent>
#include <QAction>
#include <QApplication>
#include <QScreen>
#include <QFontMetrics>
#include <QTimer>
#include <QCursor>

// ── constructor ───────────────────────────────────────────────────────────────

CosmicMenuWindow::CosmicMenuWindow(QWidget* parent)
    : QWidget(parent,
              Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    // Do NOT steal focus from the bar — bar needs to keep receiving events
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);

    mSubmenuTimer = new QTimer(this);
    mSubmenuTimer->setSingleShot(true);
    mSubmenuTimer->setInterval(150);
    connect(mSubmenuTimer, &QTimer::timeout, this, [this]() {
        if (mPendingSubmenu >= 0)
            openSubmenuAt(mPendingSubmenu);
    });
}

CosmicMenuWindow::~CosmicMenuWindow() = default;

// ── public API ────────────────────────────────────────────────────────────────

void CosmicMenuWindow::addAction(QAction* action)
{
    if (!action) return;

    MenuItem item;
    item.kind   = ItemKind::Action;
    item.action = action;
    mItems.append(item);

    connect(action, &QAction::changed, this, QOverload<>::of(&QWidget::update));
    connect(action, &QObject::destroyed, this, [this, action]() {
        mItems.erase(std::remove_if(mItems.begin(), mItems.end(),
            [action](const MenuItem& m){ return m.action == action; }),
            mItems.end());
        recalcLayout();
    });

    recalcLayout();
}

void CosmicMenuWindow::addMenu(const QString& label, CosmicMenuWindow* submenu,
                               std::function<QString()> labelFactory)
{
    if (!submenu) return;

    MenuItem item;
    item.kind         = ItemKind::Submenu;
    item.label        = label;
    item.labelFactory = std::move(labelFactory);
    item.submenu      = submenu;
    mItems.append(item);
    recalcLayout();
}

void CosmicMenuWindow::addSeparator()
{
    MenuItem item;
    item.kind = ItemKind::Separator;
    mItems.append(item);
    recalcLayout();
}

void CosmicMenuWindow::clearItems()
{
    for (const MenuItem& m : qAsConst(mItems)) {
        if (m.action)
            disconnect(m.action, nullptr, this, nullptr);
    }
    mItems.clear();
    mHoverIndex    = -1;
    mActiveSubmenu = -1;
    recalcLayout();
}

void CosmicMenuWindow::retranslate()
{
    // Re-evaluate label factories for all submenu items, then recurse into submenus.
    for (MenuItem& m : mItems) {
        if (m.kind == ItemKind::Submenu && m.labelFactory) {
            m.label = m.labelFactory();
        }
        if (m.submenu) {
            m.submenu->retranslate();
        }
    }
    recalcLayout();
    update();
}

void CosmicMenuWindow::popup(const QPoint& globalTopLeft)
{
    recalcLayout();
    move(globalTopLeft);
    show();
    raise();

    // Seed hover from current cursor position
    QPoint local = mapFromGlobal(QCursor::pos());
    mHoverIndex = itemAtPos(local);
    update();
}

void CosmicMenuWindow::closeAll()
{
    closeSubmenu();
    hide();
}

void CosmicMenuWindow::setMaximumMenuWidth(int px)
{
    mMaxWidth = qMax(kMinWidth, px);
    recalcLayout();
}

bool CosmicMenuWindow::containsGlobal(const QPoint& globalPos) const
{
    if (isVisible() && geometry().contains(globalPos))
        return true;
    // Check open child submenu recursively
    if (mActiveSubmenu >= 0) {
        const MenuItem& m = mItems[mActiveSubmenu];
        if (m.submenu && m.submenu->containsGlobal(globalPos))
            return true;
    }
    return false;
}

void CosmicMenuWindow::replaceUndoRedo(QAction* newUndo, QAction* newRedo)
{
    // Replace the first two Action items (undo then redo) with new pointers.
    int replaced = 0;
    for (MenuItem& m : mItems) {
        if (m.kind != ItemKind::Action) continue;
        if (replaced == 0) {
            if (m.action) disconnect(m.action, nullptr, this, nullptr);
            m.action = newUndo;
            if (newUndo) {
                connect(newUndo, &QAction::changed, this, QOverload<>::of(&QWidget::update));
                connect(newUndo, &QObject::destroyed, this, [this, newUndo]() {
                    mItems.erase(std::remove_if(mItems.begin(), mItems.end(),
                        [newUndo](const MenuItem& mi){ return mi.action == newUndo; }),
                        mItems.end());
                    recalcLayout();
                });
            }
            ++replaced;
        } else if (replaced == 1) {
            if (m.action) disconnect(m.action, nullptr, this, nullptr);
            m.action = newRedo;
            if (newRedo) {
                connect(newRedo, &QAction::changed, this, QOverload<>::of(&QWidget::update));
                connect(newRedo, &QObject::destroyed, this, [this, newRedo]() {
                    mItems.erase(std::remove_if(mItems.begin(), mItems.end(),
                        [newRedo](const MenuItem& mi){ return mi.action == newRedo; }),
                        mItems.end());
                    recalcLayout();
                });
            }
            ++replaced;
            break;
        }
    }
    recalcLayout();
}

void CosmicMenuWindow::updateHoverFromGlobal(const QPoint& globalPos)
{
    // If this window contains the cursor, update its hover
    if (isVisible() && geometry().contains(globalPos)) {
        QPoint local = mapFromGlobal(globalPos);
        int newHover = itemAtPos(local);
        if (newHover != mHoverIndex) {
            mHoverIndex = newHover;
            update();
        }
        return;
    }
    // Recurse into open child submenu
    if (mActiveSubmenu >= 0) {
        const MenuItem& m = mItems[mActiveSubmenu];
        if (m.submenu)
            m.submenu->updateHoverFromGlobal(globalPos);
    }
}

// ── layout ────────────────────────────────────────────────────────────────────

QSize CosmicMenuWindow::computePreferredSize() const
{
    QFontMetrics fm(font());
    int maxTextWidth = 0;

    for (const MenuItem& m : mItems) {
        if (m.kind == ItemKind::Separator) continue;

        QString text = (m.kind == ItemKind::Action) ? m.action->text() : m.label;
        int w = fm.horizontalAdvance(text);

        if (m.kind == ItemKind::Action && m.action && !m.action->shortcut().isEmpty()) {
            QString sc = m.action->shortcut().toString(QKeySequence::NativeText);
            w += kShortcutGap + fm.horizontalAdvance(sc);
        }
        if (m.kind == ItemKind::Submenu)
            w += kShortcutGap + 12;

        maxTextWidth = qMax(maxTextWidth, w);
    }

    int totalWidth = kToggleAreaWidth + kTextLeftPad + maxTextWidth + kTextRightPad;
    totalWidth = qBound(kMinWidth, totalWidth, mMaxWidth);

    int totalHeight = kWindowVPad * 2;
    for (const MenuItem& m : mItems) {
        totalHeight += (m.kind == ItemKind::Separator)
            ? kSeparatorVMargin * 2 + 1
            : kRowMinHeight;
    }

    return QSize(totalWidth, totalHeight);
}

void CosmicMenuWindow::recalcLayout()
{
    QSize sz = computePreferredSize();
    resize(sz);

    int y = kWindowVPad;
    for (MenuItem& m : mItems) {
        int h = (m.kind == ItemKind::Separator)
                ? kSeparatorVMargin * 2 + 1
                : kRowMinHeight;
        m.rowRect = QRect(0, y, sz.width(), h);
        y += h;
    }
}

// ── painting ──────────────────────────────────────────────────────────────────

void CosmicMenuWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int r = CosmicUiMetrics::windowCornerRadius();
    QPainterPath path;
    path.addRoundedRect(rect(), r, r);

    p.fillPath(path, CosmicColor::menuWindowBackground());
    p.setPen(QPen(CosmicColor::menuWindowBorder(), 1));
    p.drawPath(path);

    for (int i = 0; i < mItems.size(); ++i)
        paintItem(p, i);
}

void CosmicMenuWindow::paintItem(QPainter& p, int index) const
{
    const MenuItem& m = mItems[index];
    const QRect& row  = m.rowRect;

    if (m.kind == ItemKind::Separator) {
        int lineY = row.top() + kSeparatorVMargin;
        p.setPen(QPen(CosmicColor::menuWindowSeparator(), 1));
        p.drawLine(kToggleAreaWidth + kTextLeftPad, lineY,
                   width() - kTextRightPad,         lineY);
        return;
    }

    // Only the item directly under the cursor is highlighted.
    // mActiveSubmenu only controls whether the child is open — it does NOT
    // add a second highlight. This prevents multiple items appearing selected.
    bool isHovered = (index == mHoverIndex);
    if (isHovered) {
        QPainterPath rowPath;
        const int rr = CosmicUiMetrics::nestedCornerRadius(CosmicUiMetrics::windowCornerRadius(), 4);
        rowPath.addRoundedRect(row.adjusted(4, 1, -4, -1), rr, rr);
        p.fillPath(rowPath, CosmicColor::accent());
    }

    bool enabled = isItemEnabled(index);
    QColor textColor;
    if (isHovered)       textColor = QColor(0xff, 0xff, 0xff);
    else if (!enabled)   textColor = CosmicColor::menuWindowItemTextDisabled();
    else                 textColor = CosmicColor::menuWindowItemText();

    // Checkmark
    if (m.kind == ItemKind::Action && m.action &&
        m.action->isCheckable() && m.action->isChecked())
    {
        p.setPen(QPen(textColor, 1.5));
        int cx = kToggleAreaWidth / 2;
        int cy = row.center().y();
        QPolygon check;
        check << QPoint(cx - 5, cy)
              << QPoint(cx - 2, cy + 4)
              << QPoint(cx + 5, cy - 4);
        p.drawPolyline(check);
    }

    // Label
    QString text = (m.kind == ItemKind::Action) ? m.action->text() : m.label;
    int textX = kToggleAreaWidth + kTextLeftPad;
    int textW = width() - textX - kTextRightPad;

    p.setPen(textColor);
    p.drawText(QRect(textX, row.top(), textW, row.height()),
               Qt::AlignVCenter | Qt::AlignLeft, text);

    // Shortcut
    if (m.kind == ItemKind::Action && m.action && !m.action->shortcut().isEmpty()) {
        QString sc = m.action->shortcut().toString(QKeySequence::NativeText);
        QColor scColor = isHovered ? QColor(0xff, 0xff, 0xff, 180)
                                   : CosmicColor::menuWindowShortcutText();
        p.setPen(scColor);
        p.drawText(QRect(textX, row.top(), textW, row.height()),
                   Qt::AlignVCenter | Qt::AlignRight, sc);
    }

    // Chevron
    if (m.kind == ItemKind::Submenu) {
        int cx = width() - kTextRightPad - 4;
        int cy = row.center().y();
        p.setPen(QPen(textColor, 1.5));
        QPolygon chevron;
        chevron << QPoint(cx - 4, cy - 4)
                << QPoint(cx,     cy)
                << QPoint(cx - 4, cy + 4);
        p.drawPolyline(chevron);
    }
}

// ── mouse events ──────────────────────────────────────────────────────────────

void CosmicMenuWindow::mouseMoveEvent(QMouseEvent* event)
{
    int newHover = itemAtPos(event->pos());

    if (newHover != mHoverIndex) {
        mHoverIndex = newHover;
        update();
    }

    if (newHover >= 0 && mItems[newHover].kind == ItemKind::Submenu) {
        if (mActiveSubmenu != newHover && mPendingSubmenu != newHover) {
            mSubmenuTimer->stop();
            mPendingSubmenu = newHover;
            mSubmenuTimer->start();
        }
    } else {
        mSubmenuTimer->stop();
        mPendingSubmenu = -1;
        if (mActiveSubmenu >= 0 && newHover != mActiveSubmenu)
            closeSubmenu();
    }
}

void CosmicMenuWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int idx = itemAtPos(event->pos());
        if (idx >= 0) {
            triggerItem(idx);
        } else {
            // Clicked inside window but not on an item — close
            if (mOwnerBar) mOwnerBar->forceCloseAll();
            else closeAll();
        }
        event->accept();
        return;
    }
    event->ignore();
}

void CosmicMenuWindow::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
}

// ── keyboard events ───────────────────────────────────────────────────────────

void CosmicMenuWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        mHoverIndex = nextEnabledItem(mHoverIndex, -1);
        update();
        event->accept();
        break;
    case Qt::Key_Down:
        mHoverIndex = nextEnabledItem(mHoverIndex, +1);
        update();
        event->accept();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        triggerItem(mHoverIndex);
        event->accept();
        break;
    case Qt::Key_Right:
        if (mHoverIndex >= 0 && mItems[mHoverIndex].kind == ItemKind::Submenu) {
            openSubmenuAt(mHoverIndex);
            event->accept();
        }
        break;
    case Qt::Key_Left:
        if (mActiveSubmenu >= 0) {
            closeSubmenu();
            event->accept();
        } else {
            hide();
            if (mOwnerBar && mBarIndex > 0)
                mOwnerBar->openEntryAt(mBarIndex - 1);
            event->accept();
        }
        break;
    case Qt::Key_Escape:
        if (mOwnerBar) mOwnerBar->forceCloseAll();
        else closeAll();
        event->accept();
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void CosmicMenuWindow::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    mHoverIndex    = -1;
    mActiveSubmenu = -1;
    mSubmenuTimer->stop();
    mPendingSubmenu = -1;
    // NOTE: do NOT call back into the bar here.
    // forceCloseAll() on the bar is the single authority for closing.
    // hideEvent fires for submenus too — calling stopTracking from here
    // would kill the event filter while the parent menu is still open.
}

// ── submenu management ────────────────────────────────────────────────────────

void CosmicMenuWindow::openSubmenuAt(int index)
{
    if (index < 0 || index >= mItems.size()) return;
    MenuItem& item = mItems[index];
    if (item.kind != ItemKind::Submenu || !item.submenu) return;

    if (mActiveSubmenu >= 0 && mActiveSubmenu != index)
        closeSubmenu();

    mActiveSubmenu = index;
    update();

    QPoint rowGlobalTopRight = mapToGlobal(QPoint(width(), item.rowRect.top()));
    QScreen* screen = QApplication::screenAt(rowGlobalTopRight);
    if (!screen) screen = QApplication::primaryScreen();
    QRect screenRect = screen->availableGeometry();

    int childW = item.submenu->sizeHint().width();
    int childH = item.submenu->sizeHint().height();
    int x = rowGlobalTopRight.x();
    int y = rowGlobalTopRight.y();

    if (x + childW > screenRect.right())
        x = mapToGlobal(QPoint(0, 0)).x() - childW;
    y = qBound(screenRect.top(), y, screenRect.bottom() - childH);

    item.submenu->mOwnerBar = mOwnerBar;
    item.submenu->mBarIndex = mBarIndex;
    item.submenu->popup(QPoint(x, y));
}

void CosmicMenuWindow::closeSubmenu()
{
    if (mActiveSubmenu >= 0) {
        MenuItem& item = mItems[mActiveSubmenu];
        if (item.submenu && item.submenu->isVisible())
            item.submenu->closeAll();
        mActiveSubmenu = -1;
        update();
    }
}

// ── helpers ───────────────────────────────────────────────────────────────────

int CosmicMenuWindow::itemAtPos(const QPoint& localPos) const
{
    for (int i = 0; i < mItems.size(); ++i) {
        if (mItems[i].rowRect.contains(localPos))
            return i;
    }
    return -1;
}

int CosmicMenuWindow::nextEnabledItem(int from, int direction) const
{
    if (mItems.isEmpty()) return -1;
    int n = mItems.size();
    int start = (from < 0) ? (direction > 0 ? -1 : n) : from;
    for (int i = 1; i <= n; ++i) {
        int idx = ((start + direction * i) % n + n) % n;
        if (mItems[idx].kind != ItemKind::Separator && isItemEnabled(idx))
            return idx;
    }
    return from;
}

bool CosmicMenuWindow::isItemEnabled(int index) const
{
    if (index < 0 || index >= mItems.size()) return false;
    const MenuItem& m = mItems[index];
    if (m.kind == ItemKind::Separator) return false;
    if (m.kind == ItemKind::Submenu)   return true;
    return m.action && m.action->isEnabled();
}

void CosmicMenuWindow::triggerItem(int index)
{
    if (index < 0 || index >= mItems.size()) return;
    const MenuItem& m = mItems[index];

    if (m.kind == ItemKind::Separator) return;

    // Submenu expander — open child, do NOT close
    if (m.kind == ItemKind::Submenu) { openSubmenuAt(index); return; }

    if (!m.action || !m.action->isEnabled()) return;

    // Capture before closing
    QAction* action = m.action;

    // Force-close the entire system first, then trigger
    if (mOwnerBar)
        mOwnerBar->forceCloseAll();
    else
        closeAll();

    action->trigger();
}

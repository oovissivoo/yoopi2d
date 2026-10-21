#include "cosmicmenubar.h"
#include "cosmicmenuwindow.h"
#include "cosmicuimetrics.h"
#include "cosmictext.h"

// Out-of-line definitions required for ODR-use in C++11/14
constexpr int CosmicMenuBar::kVerticalPadding;
constexpr int CosmicMenuBar::kEntryHPad;
constexpr int CosmicMenuBar::kLogoHPad;

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QScreen>
#include <QCursor>
// ── constructor / destructor ──────────────────────────────────────────────────

CosmicMenuBar::CosmicMenuBar(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(menuBarHeight());
    setMouseTracking(true);
}

CosmicMenuBar::~CosmicMenuBar()
{
    forceCloseAll();
}

// ── public API ────────────────────────────────────────────────────────────────

int CosmicMenuBar::menuBarHeight() const
{
    return CosmicUiMetrics::controlThickness() + kVerticalPadding * 2;
}

void CosmicMenuBar::addEntry(const QString& label, CosmicMenuWindow* menu,
                             std::function<QString()> labelFactory)
{
    Q_ASSERT_X(menu != nullptr, "CosmicMenuBar::addEntry", "menu must not be null");
    if (!menu) return;

    MenuEntry entry;
    entry.label        = label;
    entry.labelFactory = std::move(labelFactory);
    entry.menu         = menu;
    entry.enabled      = true;

    menu->mOwnerBar = this;
    menu->mBarIndex = mEntries.size();

    mEntries.append(entry);
    computeHitRects();
    update();
}

void CosmicMenuBar::removeEntry(const QString& label)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        if (mEntries[i].label == label) {
            if (mEntries[i].menu)
                mEntries[i].menu->mOwnerBar = nullptr;
            mEntries.removeAt(i);
            for (int j = i; j < mEntries.size(); ++j) {
                if (mEntries[j].menu)
                    mEntries[j].menu->mBarIndex = j;
            }
            if (mHoverIndex  >= mEntries.size()) mHoverIndex  = -1;
            if (mActiveIndex >= mEntries.size()) mActiveIndex = -1;
            computeHitRects();
            update();
            return;
        }
    }
}

int CosmicMenuBar::entryCount() const { return mEntries.size(); }

void CosmicMenuBar::setEntryEnabled(const QString& label, bool enabled)
{
    for (MenuEntry& e : mEntries) {
        if (e.label == label) { e.enabled = enabled; update(); return; }
    }
}

void CosmicMenuBar::setLogoIcon(const QPixmap& pixmap)
{
    mLogoPixmap = pixmap;
    int iconH = menuBarHeight() - kVerticalPadding * 2;
    if (!mLogoPixmap.isNull()) {
        // Scale to fit height, preserve aspect ratio
        mLogoPixmap = mLogoPixmap.scaledToHeight(iconH, Qt::SmoothTransformation);
        mLogoAreaWidth = mLogoPixmap.width() + kLogoHPad * 2;
    } else {
        mLogoAreaWidth = 0;
    }
    computeHitRects();
    update();
}

void CosmicMenuBar::setVersionLabel(const QString& label)
{
    mVersionLabel = label;
    update();
}

// ── force-close: the single authority for closing everything ─────────────────

void CosmicMenuBar::forceCloseAll()
{
    // Guard against re-entrancy (e.g. hideEvent calling back into here)
    if (mClosing) return;
    mClosing = true;

    // Hide all menu windows — closeAll() recurses into submenus
    for (MenuEntry& e : mEntries) {
        if (e.menu && e.menu->isVisible())
            e.menu->closeAll();
    }

    mActiveIndex = -1;
    mHoverIndex  = -1;
    stopTracking();
    repaint();

    mClosing = false;
}

// ── retranslation ─────────────────────────────────────────────────────────────

void CosmicMenuBar::retranslate()
{
    // Re-evaluate label factories for all top-level entries, then recurse into menus.
    for (MenuEntry& e : mEntries) {
        if (e.labelFactory) {
            e.label = e.labelFactory();
        }
        if (e.menu) {
            e.menu->retranslate();
        }
    }
    computeHitRects();
    update();
}

void CosmicMenuBar::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
    }
}

// ── tracking (event filter + poll timer) ─────────────────────────────────────

void CosmicMenuBar::startTracking()
{
    if (!mTrackingInstalled) {
        qApp->installEventFilter(this);
        mTrackingInstalled = true;
    }
    if (!mPollTimer) {
        mPollTimer = new QTimer(this);
        mPollTimer->setInterval(50);
        connect(mPollTimer, &QTimer::timeout, this, [this]() {
            if (mActiveIndex < 0) { stopTracking(); return; }
            if (!isInsideMenuSystem(QCursor::pos()))
                forceCloseAll();
        });
    }
    mPollTimer->start();
}

void CosmicMenuBar::stopTracking()
{
    if (mTrackingInstalled) {
        qApp->removeEventFilter(this);
        mTrackingInstalled = false;
    }
    if (mPollTimer)
        mPollTimer->stop();
}

// ── layout ────────────────────────────────────────────────────────────────────

void CosmicMenuBar::computeHitRects()
{
    QFontMetrics fm(CosmicText::font());
    int x = mLogoAreaWidth;  // entries start after the logo
    int h = menuBarHeight();
    for (MenuEntry& e : mEntries) {
        int textW = fm.horizontalAdvance(e.label);
        int rectW = textW + kEntryHPad * 2;
        e.hitRect = QRect(x, 0, rectW, h);
        x += rectW;
    }
}

int CosmicMenuBar::entryAtPos(const QPoint& pos) const
{
    for (int i = 0; i < mEntries.size(); ++i) {
        if (mEntries[i].hitRect.contains(pos))
            return i;
    }
    return -1;
}

bool CosmicMenuBar::isInsideMenuSystem(const QPoint& globalPos) const
{
    // Bar strip
    if (QRect(mapToGlobal(QPoint(0,0)), size()).contains(globalPos))
        return true;
    // Any open menu (containsGlobal recurses into open submenus)
    for (const MenuEntry& e : mEntries) {
        if (e.menu && e.menu->containsGlobal(globalPos))
            return true;
    }
    return false;
}

// ── event filter ─────────────────────────────────────────────────────────────

bool CosmicMenuBar::eventFilter(QObject* obj, QEvent* event)
{
    Q_UNUSED(obj)

    if (event->type() == QEvent::MouseMove) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        QPoint gp = me->globalPos();

        // Over the bar?
        QRect barRect(mapToGlobal(QPoint(0,0)), size());
        if (barRect.contains(gp)) {
            QPoint lp = mapFromGlobal(gp);
            int newHover = entryAtPos(lp);
            if (newHover != mHoverIndex) {
                mHoverIndex = newHover;
                update();
            }
            // Switch to hovered entry if a different menu is open
            if (newHover >= 0 && newHover != mActiveIndex) {
                forceCloseAll();
                openEntryAt(newHover);
            }
            return false;
        }

        // Over an open menu or submenu?
        if (isInsideMenuSystem(gp)) {
            // Route hover into whichever menu window the cursor is over
            for (const MenuEntry& e : mEntries) {
                if (e.menu)
                    e.menu->updateHoverFromGlobal(gp);
            }
            return false;
        }

        // Outside everything — close
        forceCloseAll();
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        QPoint gp = me->globalPos();

        // Click on a menu window → let the menu's mousePressEvent handle it
        // (it will call forceCloseAll via triggerItem or directly)
        for (const MenuEntry& e : mEntries) {
            if (e.menu && e.menu->containsGlobal(gp))
                return false;  // don't intercept — menu handles it
        }

        // Click on bar → let bar's mousePressEvent handle it (toggle logic)
        if (QRect(mapToGlobal(QPoint(0,0)), size()).contains(gp))
            return false;

        // Click anywhere else (canvas, other widgets) → force close
        forceCloseAll();
        return false;
    }

    return false;
}

// ── painting ──────────────────────────────────────────────────────────────────

void CosmicMenuBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), CosmicColor::menuBarBackground());

    // Draw logo icon as the first decorative element (non-interactive), tinted white
    if (!mLogoPixmap.isNull()) {
        int iconY = (height() - mLogoPixmap.height()) / 2;
        QPixmap whiteLogo(mLogoPixmap.size());
        whiteLogo.fill(Qt::transparent);
        QPainter lp(&whiteLogo);
        lp.drawPixmap(0, 0, mLogoPixmap);
        lp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        lp.fillRect(whiteLogo.rect(), Qt::white);
        lp.end();
        p.drawPixmap(kLogoHPad, iconY, whiteLogo);
    }

    const QFont f = CosmicText::font();
    p.setFont(f);
    const int r = CosmicUiMetrics::cornerRadius();

    for (int i = 0; i < mEntries.size(); ++i) {
        const MenuEntry& e = mEntries[i];

        // Active = menu is actually visible right now (single source of truth)
        bool isActive = (e.menu && e.menu->isVisible());

        QColor fillColor;
        if (isActive)              fillColor = CosmicColor::accent();
        else if (i == mHoverIndex) fillColor = CosmicColor::menuBarHover();

        if (fillColor.isValid()) {
            QPainterPath path;
            path.addRoundedRect(e.hitRect.adjusted(2, 2, -2, -2), r, r);
            p.fillPath(path, fillColor);
        }

        QColor textColor = e.enabled ? CosmicColor::menuBarLabel()
                                     : CosmicColor::menuBarLabelDisabled();
        if (isActive) textColor = QColor(0xff, 0xff, 0xff);

        p.setPen(textColor);
        p.drawText(e.hitRect, Qt::AlignCenter, e.label);
    }

    // Version label — far-right edge, accent color, bold
    if (!mVersionLabel.isEmpty()) {
        QFont vf = f;
        vf.setBold(true);
        p.setFont(vf);
        const QString vText = QStringLiteral("V.") + mVersionLabel;
        QFontMetrics fm(vf);
        int textW = fm.horizontalAdvance(vText);
        QRect vRect(width() - textW - kEntryHPad * 2, 0, textW + kEntryHPad, height());
        p.setPen(CosmicColor::accent());
        p.drawText(vRect, Qt::AlignVCenter | Qt::AlignRight, vText);
    }
}

// ── mouse events ─────────────────────────────────────────────────────────────

void CosmicMenuBar::mouseMoveEvent(QMouseEvent* event)
{
    int newHover = entryAtPos(event->pos());
    if (newHover != mHoverIndex) {
        mHoverIndex = newHover;
        update();
        if (mActiveIndex >= 0 && newHover >= 0 && newHover != mActiveIndex) {
            forceCloseAll();
            openEntryAt(newHover);
        }
    }
}

void CosmicMenuBar::enterEvent(QEvent*)
{
    QPoint local = mapFromGlobal(QCursor::pos());
    int newHover = entryAtPos(local);
    if (newHover != mHoverIndex) {
        mHoverIndex = newHover;
        update();
    }
}

void CosmicMenuBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int idx = entryAtPos(event->pos());
        if (idx >= 0 && mEntries[idx].enabled) {
            if (mActiveIndex == idx) {
                // Toggle: same entry clicked while open → close
                forceCloseAll();
            } else {
                forceCloseAll();
                openEntryAt(idx);
            }
            event->accept();
            return;
        }
    }
    event->ignore();
}

void CosmicMenuBar::leaveEvent(QEvent*)
{
    if (mActiveIndex < 0) {
        mHoverIndex = -1;
        update();
    }
}

// ── keyboard events ───────────────────────────────────────────────────────────

void CosmicMenuBar::keyPressEvent(QKeyEvent* event)
{
    const int n = mEntries.size();
    if (n == 0) { QWidget::keyPressEvent(event); return; }

    switch (event->key()) {
    case Qt::Key_Left: {
        bool wasOpen = (mActiveIndex >= 0);
        int next = ((mActiveIndex < 0 ? 0 : mActiveIndex) - 1 + n) % n;
        forceCloseAll();
        mActiveIndex = next;
        update();
        if (wasOpen) openEntryAt(mActiveIndex);
        event->accept();
        break;
    }
    case Qt::Key_Right: {
        bool wasOpen = (mActiveIndex >= 0);
        int next = ((mActiveIndex < 0 ? -1 : mActiveIndex) + 1) % n;
        forceCloseAll();
        mActiveIndex = next;
        update();
        if (wasOpen) openEntryAt(mActiveIndex);
        event->accept();
        break;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (mActiveIndex >= 0)     openEntryAt(mActiveIndex);
        else if (mHoverIndex >= 0) openEntryAt(mHoverIndex);
        event->accept();
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

// ── resize ────────────────────────────────────────────────────────────────────

void CosmicMenuBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    computeHitRects();
}

QSize CosmicMenuBar::sizeHint() const
{
    int w = parentWidget() ? parentWidget()->width() : 400;
    return QSize(w, menuBarHeight());
}

// ── open entry ────────────────────────────────────────────────────────────────

void CosmicMenuBar::openEntryAt(int index)
{
    if (index < 0 || index >= mEntries.size()) return;
    MenuEntry& e = mEntries[index];
    if (!e.enabled || !e.menu) return;

    mActiveIndex = index;
    update();

    QPoint barBottomLeft = mapToGlobal(QPoint(e.hitRect.left(), height()));
    QScreen* screen = QApplication::screenAt(barBottomLeft);
    if (!screen) screen = QApplication::primaryScreen();
    QRect screenRect = screen->availableGeometry();

    int x = qBound(screenRect.left(),
                   barBottomLeft.x(),
                   screenRect.right() - e.menu->sizeHint().width());

    e.menu->popup(QPoint(x, barBottomLeft.y()));
    startTracking();
    emit entryActivated(e.label);
}

// kept for CosmicMenuWindow back-compat (keyboard nav)
void CosmicMenuBar::closeActiveMenu()
{
    forceCloseAll();
}

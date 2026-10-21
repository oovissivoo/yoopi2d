/*
Popup Color Wheel Widget Implementation
A simplified popup version of the ColorBox that only includes the color wheel,
preview, hex input, and wheel toggle - without the slider functionality.
Automatically closes when the mouse leaves the widget.
*/

#include "popupcolorwheelwidget.h"
#include "cosmiccolorwheelwidget.h"
#include "preferencemanager.h"
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QShowEvent>
#include <QMouseEvent>
#include <QCursor>
#include <QSizePolicy>
#include <QtGlobal>

#include <QGraphicsDropShadowEffect>

PopupColorWheelWidget::PopupColorWheelWidget(QWidget* parent)
    : CosmicToolsPane(tr("Color Wheel"), CosmicToolsPane::Side::Right, nullptr)
{
    Q_UNUSED(parent);
    setupUI();
    setupConnections();
    
    // Auto-close timer is kept only as a safety net for edge cases (e.g. mouse warp).
    // Primary close path is region-based via the app-level event filter.
    mAutoCloseTimer = new QTimer(this);
    mAutoCloseTimer->setSingleShot(true);
    mAutoCloseTimer->setInterval(150);
    connect(mAutoCloseTimer, &QTimer::timeout, this, &PopupColorWheelWidget::onAutoCloseTimer);
    
    // Set window properties (floating, lightweight pane)
    // Note: must be a top-level window (no parent) to avoid native title bar + pane header double rendering.
    setWindowFlags(Qt::Window | Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowTitle(tr("Color Wheel"));
    resize(280, 320);
    setMinimumSize(220, 240);
    setAttribute(Qt::WA_DeleteOnClose, true);

    // Correct alpha blending so rounded corners don't show as opaque black.
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    // Note: avoid QGraphicsDropShadowEffect here.
    // It can create a visual region outside the widget rect that is not hit-testable,
    // leading to perceived click-through and inconsistent interaction near corners.
    setGraphicsEffect(nullptr);

    setMouseTracking(true);
    setResizeGripEnabled(false);
    setVerticalResizeGripEnabled(false);

    // Disable stack-pane interactions (collapse/transfer drag). We'll provide our own window drag.
    setFloatingPopupMode(true);
}

PopupColorWheelWidget::~PopupColorWheelWidget()
{
    setAppMouseTrackingEnabled(false);
}

void PopupColorWheelWidget::setAppMouseTrackingEnabled(bool enabled)
{
    if (enabled == mAppMouseTrackingEnabled) {
        return;
    }

    if (enabled) {
        if (qApp) {
            qApp->installEventFilter(this);
        }
        mAppMouseTrackingEnabled = true;
    } else {
        if (qApp) {
            qApp->removeEventFilter(this);
        }
        mAppMouseTrackingEnabled = false;

        // Defensive: ensure we never leave the app with an override resize cursor.
        if (mResizeCursorOverrideActive) {
            QApplication::restoreOverrideCursor();
            mResizeCursorOverrideActive = false;
        }
    }
}

void PopupColorWheelWidget::setupUI()
{
    mWheel = new CosmicColorWheelWidget(this);
    mWheel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setContentWidget(mWheel);

    ensureResizeHitWidgets();
    updateResizeHitWidgetsGeometry();

    // Install an event filter so border resize hit-testing works even when the cursor is over child widgets.
    mWheel->installEventFilter(this);
    const auto children = mWheel->findChildren<QWidget*>();
    for (QWidget* w : children) {
        w->installEventFilter(this);
    }
}

void PopupColorWheelWidget::ensureResizeHitWidgets()
{
    const auto makeHit = [this](Qt::CursorShape cursor) {
        auto* w = new QWidget(this);
        w->setAttribute(Qt::WA_NoSystemBackground, true);
        w->setAutoFillBackground(false);
        w->setMouseTracking(true);
        w->setCursor(cursor);
        w->installEventFilter(this);
        w->raise();
        return w;
    };

    if (!mResizeHitLeft) {
        mResizeHitLeft = makeHit(Qt::SizeHorCursor);
        mResizeHitRight = makeHit(Qt::SizeHorCursor);
        mResizeHitTop = makeHit(Qt::SizeVerCursor);
        mResizeHitBottom = makeHit(Qt::SizeVerCursor);

        mResizeHitTL = makeHit(Qt::SizeFDiagCursor);
        mResizeHitTR = makeHit(Qt::SizeBDiagCursor);
        mResizeHitBL = makeHit(Qt::SizeBDiagCursor);
        mResizeHitBR = makeHit(Qt::SizeFDiagCursor);
    }
}

void PopupColorWheelWidget::updateResizeHitWidgetsGeometry()
{
    ensureResizeHitWidgets();
    const int band = qMax(1, mResizeBandPx);
    const int corner = qMax(band, mCornerHitPx);

    // Edge bands
    mResizeHitLeft->setGeometry(0, 0, band, height());
    mResizeHitRight->setGeometry(width() - band, 0, band, height());
    mResizeHitTop->setGeometry(0, 0, width(), band);
    mResizeHitBottom->setGeometry(0, height() - band, width(), band);

    // Corner zones (square hit regions; actual corner decision is handled in hitTestResizeRegion)
    mResizeHitTL->setGeometry(0, 0, corner, corner);
    mResizeHitTR->setGeometry(width() - corner, 0, corner, corner);
    mResizeHitBL->setGeometry(0, height() - corner, corner, corner);
    mResizeHitBR->setGeometry(width() - corner, height() - corner, corner, corner);

    mResizeHitLeft->raise();
    mResizeHitRight->raise();
    mResizeHitTop->raise();
    mResizeHitBottom->raise();
    mResizeHitTL->raise();
    mResizeHitTR->raise();
    mResizeHitBL->raise();
    mResizeHitBR->raise();
}

void PopupColorWheelWidget::setupConnections()
{
    if (!mWheel) {
        return;
    }
    connect(mWheel, &CosmicColorWheelWidget::colorChanged, this, &PopupColorWheelWidget::colorChanged);
    connect(mWheel, &CosmicColorWheelWidget::colorSelected, this, &PopupColorWheelWidget::colorSelected);
}

QColor PopupColorWheelWidget::color() const
{
    return mWheel ? mWheel->color() : QColor();
}

void PopupColorWheelWidget::setColor(const QColor& color)
{
    if (mWheel) {
        mWheel->setColor(color);
    }
}

void PopupColorWheelWidget::showAt(const QPoint& position)
{
    // Calculate position to center the widget
    QPoint adjustedPos = position;
    adjustedPos.setX(adjustedPos.x() - width() / 2);
    adjustedPos.setY(adjustedPos.y() - height() / 2);
    
    // Ensure the widget stays on screen
    QRect screenGeometry = QApplication::primaryScreen()->availableGeometry();
    if (adjustedPos.x() < screenGeometry.left()) {
        adjustedPos.setX(screenGeometry.left() + 10);
    }
    if (adjustedPos.y() < screenGeometry.top()) {
        adjustedPos.setY(screenGeometry.top() + 10);
    }
    if (adjustedPos.x() + width() > screenGeometry.right()) {
        adjustedPos.setX(screenGeometry.right() - width() - 10);
    }
    if (adjustedPos.y() + height() > screenGeometry.bottom()) {
        adjustedPos.setY(screenGeometry.bottom() - height() - 10);
    }
    
    move(adjustedPos);
    show();
    raise();
    activateWindow();
    if (mWheel) {
        mWheel->setFocus();
    }
}

bool PopupColorWheelWidget::event(QEvent* e)
{
    if (e->type() == QEvent::Hide) {
        setAppMouseTrackingEnabled(false);
        if (mAutoCloseTimer) mAutoCloseTimer->stop();
        mClosePending = false;
    }

    if (e->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(e);
        updateResizeCursor(me->pos());
    }

    return CosmicToolsPane::event(e);
}

bool PopupColorWheelWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (!event) {
        return CosmicToolsPane::eventFilter(watched, event);
    }

    // App-level mouse tracking: handles auto-close (region-based) and resize drag
    // from a single authoritative path so state never gets stuck.
    if (mAppMouseTrackingEnabled) {
        switch (event->type()) {

        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            const QPoint gp = me->globalPos();

            // --- Resize drag ---
            if (mResizingWindow && (me->buttons() & Qt::LeftButton)) {
                const QPoint delta = gp - mResizeStartGlobal;
                const QRect newGeom = applyResize(mResizeStartGeom, mResizeRegion, delta);
                setGeometry(newGeom);
                // Don't consume — let Qt continue routing so child widgets still paint.
            }

            // --- Auto-close region check ---
            const bool insidePopup = frameGeometry().contains(gp);
            if (insidePopup || mResizingWindow || mDraggingWindow) {
                mClosePending = false;
                if (mAutoCloseTimer) mAutoCloseTimer->stop();
            } else if (QApplication::mouseButtons() == Qt::NoButton && !mClosePending) {
                mClosePending = true;
                if (mAutoCloseTimer) mAutoCloseTimer->start();
            }
            break;
        }

        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (!mResizingWindow && !mDraggingWindow && me->button() == Qt::LeftButton) {
                const QPoint localPos = mapFromGlobal(me->globalPos());
                const ResizeRegion region = hitTestResizeRegion(localPos);
                if (region != ResizeRegion::None) {
                    mResizingWindow = true;
                    mResizeRegion = region;
                    mResizeStartGlobal = me->globalPos();
                    mResizeStartGeom = geometry();
                    // Cancel any pending auto-close while interacting.
                    mClosePending = false;
                    if (mAutoCloseTimer) mAutoCloseTimer->stop();
                }
            }
            break;
        }

        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                if (mResizingWindow) {
                    mResizingWindow = false;
                    mResizeRegion = ResizeRegion::None;
                    // After releasing, re-evaluate whether cursor is still inside.
                    const QPoint gp2 = me->globalPos();
                    if (!frameGeometry().contains(gp2)) {
                        mClosePending = true;
                        if (mAutoCloseTimer) mAutoCloseTimer->start();
                    }
                }
                // Release any stale mouse grab this widget may hold.
                if (mouseGrabber() == this) {
                    releaseMouse();
                }
            }
            break;
        }

        default:
            break;
        }
    }

    // Resize hit widgets: only handle cursor shape — interaction is owned by the app-level path above.
    const bool isResizeHit =
        watched == mResizeHitLeft || watched == mResizeHitRight || watched == mResizeHitTop || watched == mResizeHitBottom ||
        watched == mResizeHitTL || watched == mResizeHitTR || watched == mResizeHitBL || watched == mResizeHitBR;
    if (isResizeHit) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            updateResizeCursor(mapFromGlobal(me->globalPos()));
        }
    }

    // Cursor update for child widgets (colour wheel, hex field, etc.)
    if (event->type() == QEvent::MouseMove && !isResizeHit) {
        auto* me = static_cast<QMouseEvent*>(event);
        updateResizeCursor(mapFromGlobal(me->globalPos()));
    }

    return CosmicToolsPane::eventFilter(watched, event);
}

void PopupColorWheelWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
    } else if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        if (mWheel) {
            mWheel->setFocus();
        }
    } else {
        CosmicToolsPane::keyPressEvent(event);
    }
}

void PopupColorWheelWidget::showEvent(QShowEvent* event)
{
    CosmicToolsPane::showEvent(event);
    updateResizeHitWidgetsGeometry();
    // Reset all interaction state — defensive in case a previous session left things dirty.
    mClosePending = false;
    mDraggingWindow = false;
    mResizingWindow = false;
    mResizeRegion = ResizeRegion::None;
    if (mouseGrabber() == this) releaseMouse();
    if (mAutoCloseTimer) mAutoCloseTimer->stop();
    // Start tracking global mouse position so we can do region-based close.
    setAppMouseTrackingEnabled(true);
}

void PopupColorWheelWidget::closeEvent(QCloseEvent* event)
{
    setAppMouseTrackingEnabled(false);
    if (mAutoCloseTimer) mAutoCloseTimer->stop();
    mClosePending = false;
    mDraggingWindow = false;
    mResizingWindow = false;
    mResizeRegion = ResizeRegion::None;
    if (mouseGrabber() == this) releaseMouse();

    // Disconnect all signals to prevent memory leaks and signal accumulation
    disconnect(this, nullptr, nullptr, nullptr);
    if (mWheel) {
        disconnect(mWheel, nullptr, nullptr, nullptr);
    }
    CosmicToolsPane::closeEvent(event);
}

void PopupColorWheelWidget::resizeEvent(QResizeEvent* event)
{
    CosmicToolsPane::resizeEvent(event);
    updateResizeHitWidgetsGeometry();
}

void PopupColorWheelWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        CosmicToolsPane::mousePressEvent(event);
        return;
    }

    // Resize is handled entirely by the app-level event filter — don't duplicate it here.
    // Only handle window drag via the header.
    if (!mResizingWindow) {
        QWidget* header = headerWidget();
        if (header) {
            const QPoint headerLocal = header->mapFrom(this, event->pos());
            if (header->rect().contains(headerLocal)) {
                const bool inTopResizeBand = (event->pos().y() <= mResizeBandPx);
                if (!inTopResizeBand) {
                    mDraggingWindow = true;
                    mDragGrabOffset = event->globalPos() - frameGeometry().topLeft();
                    grabMouse();
                    event->accept();
                    return;
                }
            }
        }
    }

    CosmicToolsPane::mousePressEvent(event);
}

void PopupColorWheelWidget::mouseMoveEvent(QMouseEvent* event)
{
    // Resize drag is handled by the app-level event filter.
    if (mDraggingWindow && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - mDragGrabOffset);
        event->accept();
        return;
    }

    updateResizeCursor(event->pos());
    CosmicToolsPane::mouseMoveEvent(event);
}

void PopupColorWheelWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        mDraggingWindow = false;
        // mResizingWindow is cleared by the app-level event filter's MouseButtonRelease path.

        if (mouseGrabber() == this) {
            releaseMouse();
        }

        updateResizeCursor(mapFromGlobal(QCursor::pos()));
    }
    CosmicToolsPane::mouseReleaseEvent(event);
}

void PopupColorWheelWidget::onAutoCloseTimer()
{
    if (!mClosePending) {
        return;
    }

    // Don't close while resizing/dragging or while a mouse button is held.
    if (mDraggingWindow || mResizingWindow || (QApplication::mouseButtons() != Qt::NoButton)) {
        mClosePending = false;
        return;
    }

    // Close only if the cursor is genuinely outside the popup rect.
    const QPoint gp = QCursor::pos();
    if (!frameGeometry().contains(gp)) {
        close();
    } else {
        // Cursor re-entered between the MouseMove event and the timer firing — cancel.
        mClosePending = false;
    }
}

void PopupColorWheelWidget::setPreferenceManager(PreferenceManager* manager)
{
    mPreferenceManager = manager;
}

void PopupColorWheelWidget::startLoadingFeedback()
{
}

PopupColorWheelWidget::ResizeRegion PopupColorWheelWidget::hitTestResizeRegion(const QPoint& localPos) const
{
    const QRect r = rect();

    const int band = qMax(1, mResizeBandPx);
    const int corner = qMax(band, mCornerHitPx);

    const bool inLeftBand = (localPos.x() <= r.left() + band);
    const bool inRightBand = (localPos.x() >= r.right() - band);
    const bool inTopBand = (localPos.y() <= r.top() + band);
    const bool inBottomBand = (localPos.y() >= r.bottom() - band);

    // Rounded corner zones: treat corners as quarter-circles for more natural hit-testing.
    const auto inCornerCircle = [corner](int dx, int dy) {
        const qint64 ddx = static_cast<qint64>(dx);
        const qint64 ddy = static_cast<qint64>(dy);
        return (ddx * ddx + ddy * ddy) <= static_cast<qint64>(corner) * static_cast<qint64>(corner);
    };

    const bool inTL = (localPos.x() <= r.left() + corner) && (localPos.y() <= r.top() + corner)
        && inCornerCircle(localPos.x() - r.left(), localPos.y() - r.top());
    const bool inTR = (localPos.x() >= r.right() - corner) && (localPos.y() <= r.top() + corner)
        && inCornerCircle(r.right() - localPos.x(), localPos.y() - r.top());
    const bool inBL = (localPos.x() <= r.left() + corner) && (localPos.y() >= r.bottom() - corner)
        && inCornerCircle(localPos.x() - r.left(), r.bottom() - localPos.y());
    const bool inBR = (localPos.x() >= r.right() - corner) && (localPos.y() >= r.bottom() - corner)
        && inCornerCircle(r.right() - localPos.x(), r.bottom() - localPos.y());

    // Corners are circular-ish in your diagram; prioritise them when inside the corner zone.
    if (inTL) return ResizeRegion::TopLeft;
    if (inTR) return ResizeRegion::TopRight;
    if (inBL) return ResizeRegion::BottomLeft;
    if (inBR) return ResizeRegion::BottomRight;

    if (inLeftBand) return ResizeRegion::Left;
    if (inRightBand) return ResizeRegion::Right;
    if (inTopBand) return ResizeRegion::Top;
    if (inBottomBand) return ResizeRegion::Bottom;

    return ResizeRegion::None;
}

void PopupColorWheelWidget::updateResizeCursor(const QPoint& localPos)
{
    if (mResizingWindow) {
        return;
    }

    const ResizeRegion region = hitTestResizeRegion(localPos);

    Qt::CursorShape shape = Qt::ArrowCursor;
    switch (region) {
    case ResizeRegion::TopLeft:
    case ResizeRegion::BottomRight:
        shape = Qt::SizeFDiagCursor;
        break;
    case ResizeRegion::TopRight:
    case ResizeRegion::BottomLeft:
        shape = Qt::SizeBDiagCursor;
        break;
    case ResizeRegion::Left:
    case ResizeRegion::Right:
        shape = Qt::SizeHorCursor;
        break;
    case ResizeRegion::Top:
    case ResizeRegion::Bottom:
        shape = Qt::SizeVerCursor;
        break;
    case ResizeRegion::None:
    default:
        shape = Qt::ArrowCursor;
        break;
    }

    // Cursor updates are primarily handled by the dedicated resize hit widgets.
    // This fallback only affects the pane itself (not child widgets) and does not use app-level overrides.
    if (shape != Qt::ArrowCursor) {
        setCursor(shape);
    } else {
        unsetCursor();
    }
}

QRect PopupColorWheelWidget::applyResize(const QRect& startGeom, ResizeRegion region, const QPoint& delta) const
{
    QRect g = startGeom;

    const int minW = qMax(minimumWidth(), 220);
    const int minH = qMax(minimumHeight(), 240);

    switch (region) {
    case ResizeRegion::Left:
    case ResizeRegion::TopLeft:
    case ResizeRegion::BottomLeft:
        g.setLeft(g.left() + delta.x());
        break;
    default:
        break;
    }

    switch (region) {
    case ResizeRegion::Right:
    case ResizeRegion::TopRight:
    case ResizeRegion::BottomRight:
        g.setRight(g.right() + delta.x());
        break;
    default:
        break;
    }

    switch (region) {
    case ResizeRegion::Top:
    case ResizeRegion::TopLeft:
    case ResizeRegion::TopRight:
        g.setTop(g.top() + delta.y());
        break;
    default:
        break;
    }

    switch (region) {
    case ResizeRegion::Bottom:
    case ResizeRegion::BottomLeft:
    case ResizeRegion::BottomRight:
        g.setBottom(g.bottom() + delta.y());
        break;
    default:
        break;
    }

    if (g.width() < minW) {
        if (region == ResizeRegion::Left || region == ResizeRegion::TopLeft || region == ResizeRegion::BottomLeft) {
            g.setLeft(g.right() - minW);
        } else {
            g.setRight(g.left() + minW);
        }
    }

    if (g.height() < minH) {
        if (region == ResizeRegion::Top || region == ResizeRegion::TopLeft || region == ResizeRegion::TopRight) {
            g.setTop(g.bottom() - minH);
        } else {
            g.setBottom(g.top() + minH);
        }
    }

    return g;
}
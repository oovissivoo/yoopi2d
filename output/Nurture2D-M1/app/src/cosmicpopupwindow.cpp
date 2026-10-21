#include "cosmicpopupwindow.h"

#include "../CUI/cosmicuimetrics.h"
#include "platformhandler.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSettings>
#include <QTimer>

namespace {
QRect availableScreenGeometryForGlobalPoint(const QPoint& globalPos)
{
    if (QScreen* s = QGuiApplication::screenAt(globalPos)) {
        return s->availableGeometry();
    }
    if (QGuiApplication::primaryScreen()) {
        return QGuiApplication::primaryScreen()->availableGeometry();
    }
    return QRect(0, 0, 1920, 1080);
}
}

CosmicPopupWindow::CosmicPopupWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Only enable translucent background when a compositor is running.
    // On X11 without a compositor, WA_TranslucentBackground causes the window's
    // transparent regions to show raw framebuffer content from other apps.
    const bool compositorAvailable = PlatformHandler::isCompositorActive();
    if (compositorAvailable) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
    }
    mCompositorAvailable = compositorAvailable;

    setGraphicsEffect(nullptr);

    setMouseTracking(true);

    mAutoCloseTimer = new QTimer(this);
    mAutoCloseTimer->setSingleShot(true);
    mAutoCloseTimer->setInterval(mAutoCloseDelayMs);
    connect(mAutoCloseTimer, &QTimer::timeout, this, [this]() {
        if (mClosePolicy == ClosePolicy::CloseOnMouseLeave) {
            close();
        }
    });

    // Default: manual close, draggable, non-resizable.
    setModalMode(ModalMode::NonModal);
    setClosePolicy(ClosePolicy::Manual);
    setResizable(false);

    mPersistSizeDebounce = new QTimer(this);
    mPersistSizeDebounce->setSingleShot(true);
    mPersistSizeDebounce->setInterval(350);
    connect(mPersistSizeDebounce, &QTimer::timeout, this, [this]() {
        persistSizeNow();
    });

    // Needed so resize hit widgets work even when cursor is over child widgets.
    installEventFilter(this);

    syncTemplateMetrics();
}

CosmicPopupWindow::~CosmicPopupWindow() = default;

void CosmicPopupWindow::ensurePanelPath()
{
    const int pad = mShadowEnabled ? mShadowPad : 0;
    if (!mPanelPathDirty
        && mPanelPathRect == rect()
        && mPanelPathPad == pad
        && mPanelPathRadius == mPanelRadius
        && mPanelPathBorderWidth == mBorderWidth) {
        return;
    }

    mPanelPathDirty = false;
    mPanelPathRect = rect();
    mPanelPathPad = pad;
    mPanelPathRadius = mPanelRadius;
    mPanelPathBorderWidth = mBorderWidth;

    const QRectF panelRect = QRectF(rect()).adjusted(pad, pad, -pad, -pad);
    mPanelFillRect = panelRect.adjusted(mBorderWidth * 0.5,
                                        mBorderWidth * 0.5,
                                        -mBorderWidth * 0.5,
                                        -mBorderWidth * 0.5);

    mPanelPath = QPainterPath();
    mPanelPath.addRoundedRect(mPanelFillRect, mPanelRadius, mPanelRadius);
}

void CosmicPopupWindow::setModalMode(ModalMode mode)
{
    if (mModalMode == mode) {
        return;
    }

    mModalMode = mode;

    // Auto-close popups should not be modal.
    if (mClosePolicy == ClosePolicy::CloseOnMouseLeave && mModalMode != ModalMode::NonModal) {
        mModalMode = ModalMode::NonModal;
    }

    switch (mModalMode) {
    case ModalMode::NonModal:
        setModal(false);
        setWindowModality(Qt::NonModal);
        break;
    case ModalMode::WindowModal:
        setModal(true);
        setWindowModality(Qt::WindowModal);
        break;
    case ModalMode::ApplicationModal:
        setModal(true);
        setWindowModality(Qt::ApplicationModal);
        break;
    }
}

void CosmicPopupWindow::setClosePolicy(ClosePolicy policy)
{
    if (mClosePolicy == policy) {
        return;
    }
    mClosePolicy = policy;

    if (mClosePolicy == ClosePolicy::Manual) {
        stopAutoCloseTimer();
    } else {
        // Auto-close popups must be non-modal to avoid trapping the user in a modal they can't reach.
        if (mModalMode != ModalMode::NonModal) {
            setModalMode(ModalMode::NonModal);
        }
    }
}

void CosmicPopupWindow::setAutoCloseDelayMs(int ms)
{
    ms = qMax(0, ms);
    mAutoCloseDelayMs = ms;
    if (mAutoCloseTimer) {
        mAutoCloseTimer->setInterval(mAutoCloseDelayMs);
    }
}

void CosmicPopupWindow::setResizable(bool on)
{
    if (mResizable == on) {
        return;
    }
    mResizable = on;

    if (!mResizable) {
        mResizingWindow = false;
        mResizeRegion = ResizeRegion::None;
    }

    ensureResizeHitWidgets();
    updateResizeHitWidgetsGeometry();

    const bool hitOn = mResizable && !mHasFixedWindowSize;
    QWidget* hits[] = {mResizeHitLeft, mResizeHitRight, mResizeHitTop, mResizeHitBottom, mResizeHitTL, mResizeHitTR, mResizeHitBL, mResizeHitBR};
    for (QWidget* w : hits) {
        if (!w) continue;
        w->setVisible(hitOn);
        w->setEnabled(hitOn);
    }
}

void CosmicPopupWindow::setFixedWindowSize(const QSize& sz)
{
    if (!sz.isValid()) {
        return;
    }
    mHasFixedWindowSize = true;
    mFixedWindowSize = sz;
    setFixedSize(sz);

    // Fixed size implies not resizable.
    setResizable(false);
}

void CosmicPopupWindow::clearFixedWindowSize()
{
    if (!mHasFixedWindowSize) {
        return;
    }

    mHasFixedWindowSize = false;
    mFixedWindowSize = QSize();

    setMinimumSize(QSize(0, 0));
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    // Caller can re-enable resize explicitly if desired.
    update();
}

void CosmicPopupWindow::setPanelRadius(int px)
{
    px = qMax(0, px);
    if (mPanelRadius == px) return;
    mPanelRadius = px;
    mPanelPathDirty = true;
    update();
}

void CosmicPopupWindow::setBorderWidth(int px)
{
    px = qMax(0, px);
    if (mBorderWidth == px) return;
    mBorderWidth = px;
    mPanelPathDirty = true;
    update();
}

void CosmicPopupWindow::setPanelColor(const QColor& c)
{
    if (mPanelColor == c) return;
    mPanelColor = c;
    update();
}

void CosmicPopupWindow::setBorderColor(const QColor& c)
{
    if (mBorderColor == c) return;
    mBorderColor = c;
    update();
}

void CosmicPopupWindow::setShadowEnabled(bool on)
{
    if (mShadowEnabled == on) return;
    mShadowEnabled = on;
    mPanelPathDirty = true;
    update();
}

void CosmicPopupWindow::setShadowPadding(int px)
{
    px = qMax(0, px);
    if (mShadowPad == px) return;
    mShadowPad = px;
    mPanelPathDirty = true;
    update();
}

void CosmicPopupWindow::setShadowStrength(int alpha0to255)
{
    alpha0to255 = qBound(0, alpha0to255, 255);
    if (mShadowStrength == alpha0to255) return;
    mShadowStrength = alpha0to255;
    update();
}

void CosmicPopupWindow::setDraggable(bool on)
{
    if (mDraggable == on) return;
    mDraggable = on;
}

void CosmicPopupWindow::setDragRegionHeight(int px)
{
    px = qMax(0, px);
    if (mDragRegionHeight == px) return;
    mDragRegionHeight = px;
}

void CosmicPopupWindow::setCenterOnShow(bool on)
{
    mCenterOnShow = on;
}

void CosmicPopupWindow::setPersistentSizeKey(const QString& key)
{
    if (mPersistentSizeKey == key) {
        return;
    }

    mPersistentSizeKey = key;

    if (mPersistentSizeKey.isEmpty()) {
        return;
    }

    QSettings settings;
    const QVariant v = settings.value(mPersistentSizeKey);
    if (v.isValid()) {
        const QSize s = v.toSize();
        if (s.isValid()) {
            resize(s);
        }
    }
}

void CosmicPopupWindow::schedulePersistSize()
{
    if (mPersistentSizeKey.isEmpty() || mHasFixedWindowSize) {
        return;
    }
    if (mPersistSizeDebounce) {
        mPersistSizeDebounce->start();
    }
}

void CosmicPopupWindow::persistSizeNow()
{
    if (mPersistentSizeKey.isEmpty() || mHasFixedWindowSize) {
        return;
    }

    QSettings settings;
    settings.setValue(mPersistentSizeKey, size());
}

void CosmicPopupWindow::showAt(const QPoint& globalPos, Anchor anchor, bool clampToScreen)
{
    mSkipCenterOnce = true;

    auto topLeftForAnchor = [&](Anchor a) {
        QPoint p = globalPos;
        switch (a) {
        case Anchor::Center:
            p -= QPoint(width() / 2, height() / 2);
            break;
        case Anchor::TopLeft:
            break;
        case Anchor::TopRight:
            p -= QPoint(width(), 0);
            break;
        case Anchor::BottomLeft:
            p -= QPoint(0, height());
            break;
        case Anchor::BottomRight:
            p -= QPoint(width(), height());
            break;
        }
        return p;
    };

    const QRect sg = clampToScreen ? availableScreenGeometryForGlobalPoint(globalPos) : QRect();
    const int pad = 10;
    const QRect bounds = clampToScreen ? sg.adjusted(pad, pad, -pad, -pad) : QRect();

    Anchor bestAnchor = anchor;
    QPoint bestTopLeft = topLeftForAnchor(anchor);

    if (clampToScreen) {
        const Anchor candidates[] = {
            anchor,
            Anchor::TopLeft,
            Anchor::TopRight,
            Anchor::BottomLeft,
            Anchor::BottomRight,
            Anchor::Center,
        };

        auto overflowScore = [&](const QPoint& tl) {
            const QRect r(tl, QSize(width(), height()));
            const int overL = qMax(0, bounds.left() - r.left());
            const int overT = qMax(0, bounds.top() - r.top());
            const int overR = qMax(0, r.right() - bounds.right());
            const int overB = qMax(0, r.bottom() - bounds.bottom());
            return overL + overT + overR + overB;
        };

        int bestScore = overflowScore(bestTopLeft);
        for (Anchor a : candidates) {
            const QPoint tl = topLeftForAnchor(a);
            const int score = overflowScore(tl);
            if (score < bestScore) {
                bestScore = score;
                bestAnchor = a;
                bestTopLeft = tl;
            }
        }

        Q_UNUSED(bestAnchor);
    }

    QPoint p = bestTopLeft;

    if (clampToScreen) {
        // Clamp to visible area.
        p.setX(qBound(bounds.left(), p.x(), bounds.right() - width() + 1));
        p.setY(qBound(bounds.top(), p.y(), bounds.bottom() - height() + 1));

        // Ensure the click point remains inside the popup, if possible.
        if (globalPos.x() < p.x()) {
            p.setX(globalPos.x());
        } else if (globalPos.x() > p.x() + width() - 1) {
            p.setX(globalPos.x() - width() + 1);
        }

        if (globalPos.y() < p.y()) {
            p.setY(globalPos.y());
        } else if (globalPos.y() > p.y() + height() - 1) {
            p.setY(globalPos.y() - height() + 1);
        }

        // Re-clamp after cursor-inside adjustment.
        p.setX(qBound(bounds.left(), p.x(), bounds.right() - width() + 1));
        p.setY(qBound(bounds.top(), p.y(), bounds.bottom() - height() + 1));
    }

    move(p);
    show();
    raise();
    activateWindow();
}

bool CosmicPopupWindow::event(QEvent* e)
{
    if (e && mClosePolicy == ClosePolicy::CloseOnMouseLeave) {
        if (e->type() == QEvent::Enter) {
            stopAutoCloseTimer();
        } else if (e->type() == QEvent::Leave) {
            startAutoCloseTimer();
        }
    }

    if (e && e->type() == QEvent::DynamicPropertyChange) {
        auto* pe = static_cast<QDynamicPropertyChangeEvent*>(e);
        if (pe && pe->propertyName() == QByteArrayLiteral("CosmicWindowCornerRadii")) {
            syncTemplateMetrics();
        }
    }

    // Claim Escape as a shortcut override so focused child widgets (e.g. QSlider, QAbstractItemView)
    // don't consume it before it reaches our keyPressEvent.
    if (e && e->type() == QEvent::ShortcutOverride) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Escape) {
            ke->accept();
            return true;
        }
    }

    return QDialog::event(e);
}

void CosmicPopupWindow::syncTemplateMetrics()
{
    const QVariant v = property("CosmicWindowCornerRadii");
    int r = -1;
    if (v.isValid()) {
        bool ok = false;
        r = v.toInt(&ok);
        if (!ok) {
            r = -1;
        }
    }
    if (r < 0) {
        r = CosmicUiMetrics::windowCornerRadius();
    }
    r = qMax(0, r);

    if (mPanelRadius != r) {
        mPanelRadius = r;
        mPanelPathDirty = true;
        update();
    }
}

bool CosmicPopupWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (!event) {
        return QDialog::eventFilter(watched, event);
    }

    const bool isResizeHit =
        watched == mResizeHitLeft || watched == mResizeHitRight || watched == mResizeHitTop || watched == mResizeHitBottom ||
        watched == mResizeHitTL || watched == mResizeHitTR || watched == mResizeHitBL || watched == mResizeHitBR;

    // After grabMouse(), Qt delivers subsequent mouse move/release events to the grabber
    // (this dialog), not the original edge hit widget. Continue handling resize while active.
    const bool isResizeTrack = mResizingWindow && (watched == this);

    if ((isResizeHit || isResizeTrack) && mResizable && !mHasFixedWindowSize) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QPoint localPos = mapFromGlobal(me->globalPos());
                const ResizeRegion region = hitTestResizeRegion(localPos);
                if (region != ResizeRegion::None) {
                    mResizingWindow = true;
                    mResizeRegion = region;
                    mResizeStartGlobal = me->globalPos();
                    mResizeStartGeom = geometry();
                    grabMouse();
                    event->accept();
                    return true;
                }
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mResizingWindow && (me->buttons() & Qt::LeftButton)) {
                const QPoint delta = me->globalPos() - mResizeStartGlobal;
                const QRect newGeom = applyResize(mResizeStartGeom, mResizeRegion, delta);
                setGeometry(newGeom);
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (mResizingWindow && me->button() == Qt::LeftButton) {
                mResizingWindow = false;
                mResizeRegion = ResizeRegion::None;
                releaseMouse();
                event->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return QDialog::eventFilter(watched, event);
}

void CosmicPopupWindow::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    ensurePanelPath();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const int pad = mShadowEnabled ? mShadowPad : 0;
    const QRectF panelRect = QRectF(rect()).adjusted(pad, pad, -pad, -pad);

    if (!mCompositorAvailable) {
        // No compositor: fill the entire widget with the panel color so there
        // are no transparent holes that expose other apps' framebuffer content.
        p.fillRect(rect(), mPanelColor);
        if (mBorderWidth > 0 && mBorderColor.alpha() > 0) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(mBorderColor, mBorderWidth));
            p.drawRect(QRectF(rect()).adjusted(mBorderWidth * 0.5, mBorderWidth * 0.5,
                                               -mBorderWidth * 0.5, -mBorderWidth * 0.5));
        }
        return;
    }

    if (mShadowEnabled && mShadowStrength > 0) {
        const int shadowSteps = 10;
        for (int i = shadowSteps; i >= 1; --i) {
            const qreal t = static_cast<qreal>(i) / static_cast<qreal>(shadowSteps);
            const int a = qBound(0, static_cast<int>(static_cast<qreal>(mShadowStrength) * t * t), 255);
            const QRectF sr = panelRect.adjusted(-i, -i, i, i);
            p.setPen(QPen(QColor(0, 0, 0, a), 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(sr, mPanelRadius + i, mPanelRadius + i);
        }
    }

    p.setPen(Qt::NoPen);
    p.setBrush(mPanelColor);
    p.drawPath(mPanelPath);

    if (mBorderWidth > 0 && mBorderColor.alpha() > 0) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(mBorderColor, mBorderWidth));
        p.drawPath(mPanelPath);
    }
}

void CosmicPopupWindow::showEvent(QShowEvent* e)
{
    QDialog::showEvent(e);

    if (mCenterOnShow && !mSkipCenterOnce) {
        const QPoint globalRef = parentWidget() ? parentWidget()->mapToGlobal(parentWidget()->rect().center()) : QCursor::pos();
        const QRect sg = availableScreenGeometryForGlobalPoint(globalRef);
        const QPoint center = sg.center();
        const QPoint tl = center - QPoint(width() / 2, height() / 2);
        move(tl);
    }
    mSkipCenterOnce = false;

    updateResizeHitWidgetsGeometry();
    stopAutoCloseTimer();
}

void CosmicPopupWindow::closeEvent(QCloseEvent* e)
{
    persistSizeNow();
    stopAutoCloseTimer();
    QDialog::closeEvent(e);
}

void CosmicPopupWindow::keyPressEvent(QKeyEvent* e)
{
    // Escape closes any CosmicPopupWindow by default.
    if (e && e->key() == Qt::Key_Escape) {
        e->accept();
        reject();
        return;
    }
    QDialog::keyPressEvent(e);
}

void CosmicPopupWindow::resizeEvent(QResizeEvent* e)
{
    QDialog::resizeEvent(e);
    updateResizeHitWidgetsGeometry();
    schedulePersistSize();
    mPanelPathDirty = true;
}

void CosmicPopupWindow::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        if (mDraggable && QRect(0, 0, width(), mDragRegionHeight).contains(e->pos())) {
            mDraggingWindow = true;
            mDragGrabOffset = e->globalPos() - frameGeometry().topLeft();
            grabMouse();
            e->accept();
            return;
        }
    }

    QDialog::mousePressEvent(e);
}

void CosmicPopupWindow::mouseMoveEvent(QMouseEvent* e)
{
    if (mDraggingWindow && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPos() - mDragGrabOffset);
        e->accept();
        return;
    }

    QDialog::mouseMoveEvent(e);
}

void CosmicPopupWindow::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        mDraggingWindow = false;
        if (mouseGrabber() == this) {
            releaseMouse();
        }
    }

    QDialog::mouseReleaseEvent(e);
}

void CosmicPopupWindow::ensureResizeHitWidgets()
{
    const auto makeHit = [this](Qt::CursorShape cursor) {
        auto* w = new QWidget(this);
        w->setAttribute(Qt::WA_NoSystemBackground, true);
        w->setAutoFillBackground(false);
        w->setMouseTracking(true);
        w->setCursor(cursor);
        w->installEventFilter(this);
        w->raise();
        w->setVisible(false);
        w->setEnabled(false);
        return w;
    };

    if (!mResizeHitLeft) mResizeHitLeft = makeHit(Qt::SizeHorCursor);
    if (!mResizeHitRight) mResizeHitRight = makeHit(Qt::SizeHorCursor);
    if (!mResizeHitTop) mResizeHitTop = makeHit(Qt::SizeVerCursor);
    if (!mResizeHitBottom) mResizeHitBottom = makeHit(Qt::SizeVerCursor);

    if (!mResizeHitTL) mResizeHitTL = makeHit(Qt::SizeFDiagCursor);
    if (!mResizeHitTR) mResizeHitTR = makeHit(Qt::SizeBDiagCursor);
    if (!mResizeHitBL) mResizeHitBL = makeHit(Qt::SizeBDiagCursor);
    if (!mResizeHitBR) mResizeHitBR = makeHit(Qt::SizeFDiagCursor);
}

void CosmicPopupWindow::updateResizeHitWidgetsGeometry()
{
    ensureResizeHitWidgets();

    if (!mResizeHitLeft || !mResizeHitRight || !mResizeHitTop || !mResizeHitBottom
        || !mResizeHitTL || !mResizeHitTR || !mResizeHitBL || !mResizeHitBR) {
        return;
    }

    const bool hitOn = mResizable && !mHasFixedWindowSize;
    QWidget* hits[] = {mResizeHitLeft, mResizeHitRight, mResizeHitTop, mResizeHitBottom, mResizeHitTL, mResizeHitTR, mResizeHitBL, mResizeHitBR};
    for (QWidget* w : hits) {
        if (!w) continue;
        w->setVisible(hitOn);
        w->setEnabled(hitOn);
    }

    if (!hitOn) {
        return;
    }

    const int band = qMax(1, mResizeBandPx);
    const int corner = qMax(band, mCornerHitPx);

    mResizeHitLeft->setGeometry(0, 0, band, height());
    mResizeHitRight->setGeometry(width() - band, 0, band, height());
    mResizeHitTop->setGeometry(0, 0, width(), band);
    mResizeHitBottom->setGeometry(0, height() - band, width(), band);

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

CosmicPopupWindow::ResizeRegion CosmicPopupWindow::hitTestResizeRegion(const QPoint& localPos) const
{
    if (!mResizable || mHasFixedWindowSize) {
        return ResizeRegion::None;
    }

    const int band = qMax(1, mResizeBandPx);
    const int corner = qMax(band, mCornerHitPx);

    const bool left = localPos.x() <= band;
    const bool right = localPos.x() >= width() - band;
    const bool top = localPos.y() <= band;
    const bool bottom = localPos.y() >= height() - band;

    const bool inTL = (localPos.x() <= corner && localPos.y() <= corner);
    const bool inTR = (localPos.x() >= width() - corner && localPos.y() <= corner);
    const bool inBL = (localPos.x() <= corner && localPos.y() >= height() - corner);
    const bool inBR = (localPos.x() >= width() - corner && localPos.y() >= height() - corner);

    if (inTL) return ResizeRegion::TopLeft;
    if (inTR) return ResizeRegion::TopRight;
    if (inBL) return ResizeRegion::BottomLeft;
    if (inBR) return ResizeRegion::BottomRight;

    if (left) return ResizeRegion::Left;
    if (right) return ResizeRegion::Right;
    if (top) return ResizeRegion::Top;
    if (bottom) return ResizeRegion::Bottom;

    return ResizeRegion::None;
}

QRect CosmicPopupWindow::applyResize(const QRect& startGeom, ResizeRegion region, const QPoint& delta) const
{
    QRect g = startGeom;

    const QSize minSz = minimumSize();
    const QSize maxSz = maximumSize();

    auto clampW = [&](int w) {
        if (minSz.width() > 0) w = qMax(w, minSz.width());
        if (maxSz.width() < QWIDGETSIZE_MAX) w = qMin(w, maxSz.width());
        return w;
    };
    auto clampH = [&](int h) {
        if (minSz.height() > 0) h = qMax(h, minSz.height());
        if (maxSz.height() < QWIDGETSIZE_MAX) h = qMin(h, maxSz.height());
        return h;
    };

    switch (region) {
    case ResizeRegion::Left: {
        const int newX = startGeom.x() + delta.x();
        const int newW = clampW(startGeom.right() - newX + 1);
        g.setX(startGeom.right() - newW + 1);
        g.setWidth(newW);
        break;
    }
    case ResizeRegion::Right: {
        const int newW = clampW(startGeom.width() + delta.x());
        g.setWidth(newW);
        break;
    }
    case ResizeRegion::Top: {
        const int newY = startGeom.y() + delta.y();
        const int newH = clampH(startGeom.bottom() - newY + 1);
        g.setY(startGeom.bottom() - newH + 1);
        g.setHeight(newH);
        break;
    }
    case ResizeRegion::Bottom: {
        const int newH = clampH(startGeom.height() + delta.y());
        g.setHeight(newH);
        break;
    }
    case ResizeRegion::TopLeft: {
        const int newX = startGeom.x() + delta.x();
        const int newY = startGeom.y() + delta.y();
        const int newW = clampW(startGeom.right() - newX + 1);
        const int newH = clampH(startGeom.bottom() - newY + 1);
        g.setX(startGeom.right() - newW + 1);
        g.setY(startGeom.bottom() - newH + 1);
        g.setSize(QSize(newW, newH));
        break;
    }
    case ResizeRegion::TopRight: {
        const int newY = startGeom.y() + delta.y();
        const int newW = clampW(startGeom.width() + delta.x());
        const int newH = clampH(startGeom.bottom() - newY + 1);
        g.setY(startGeom.bottom() - newH + 1);
        g.setSize(QSize(newW, newH));
        break;
    }
    case ResizeRegion::BottomLeft: {
        const int newX = startGeom.x() + delta.x();
        const int newW = clampW(startGeom.right() - newX + 1);
        const int newH = clampH(startGeom.height() + delta.y());
        g.setX(startGeom.right() - newW + 1);
        g.setSize(QSize(newW, newH));
        break;
    }
    case ResizeRegion::BottomRight: {
        const int newW = clampW(startGeom.width() + delta.x());
        const int newH = clampH(startGeom.height() + delta.y());
        g.setSize(QSize(newW, newH));
        break;
    }
    case ResizeRegion::None:
        break;
    }

    return g;
}

void CosmicPopupWindow::startAutoCloseTimer()
{
    if (mAutoCloseTimer && mClosePolicy == ClosePolicy::CloseOnMouseLeave) {
        mAutoCloseTimer->start();
    }
}

void CosmicPopupWindow::stopAutoCloseTimer()
{
    if (mAutoCloseTimer) {
        mAutoCloseTimer->stop();
    }
}

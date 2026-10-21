#include "canvascursoroverlay.h"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

CanvasCursorOverlay::CanvasCursorOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
}

void CanvasCursorOverlay::setCursorState(const CanvasCursorPainterOptions& options, const QTransform& viewTransform, qreal opacity)
{
    const QRect newRect = computeCursorRect(options, viewTransform);
    QRect dirty = newRect;
    if (mHasCursor && !mLastRect.isNull()) {
        dirty = dirty.united(mLastRect);
    }

    mOptions = options;
    mViewTransform = viewTransform;
    mOpacity = qBound<qreal>(0.0, opacity, 1.0);

    const bool wantsCursor = (options.isAdjusting || options.showCursor);
    mHasCursor = wantsCursor;

    mLastRect = newRect;

    if (!dirty.isNull()) {
        update(dirty);
    }
}

void CanvasCursorOverlay::clearCursor()
{
    if (!mHasCursor) {
        return;
    }

    const QRect dirty = mLastRect;
    mHasCursor = false;
    mOpacity = 0.0;

    if (!dirty.isNull()) {
        update(dirty);
    }
}

void CanvasCursorOverlay::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setClipRect(event->rect());
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(event->rect(), Qt::transparent);

    if (!mHasCursor || mOpacity <= 0.0) {
        return;
    }

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(mOpacity);

    mPainter.setDevicePixelRatio(mOptions.devicePixelRatio);
    mPainter.preparePainter(mOptions, mViewTransform);
    mPainter.paint(painter, event->rect());
}

QRect CanvasCursorOverlay::computeCursorRect(const CanvasCursorPainterOptions& options, const QTransform& viewTransform) const
{
    if (!(options.isAdjusting || options.showCursor)) {
        return QRect();
    }

    const qreal scaleX = qSqrt(qPow(viewTransform.m11(), 2) + qPow(viewTransform.m21(), 2));
    const qreal scaleY = qSqrt(qPow(viewTransform.m12(), 2) + qPow(viewTransform.m22(), 2));

    QRectF width = options.widthRect;
    QPointF center = viewTransform.map(width.center());

    QSizeF wSize = width.size();
    wSize.setWidth(wSize.width() * scaleX);
    wSize.setHeight(wSize.height() * scaleY);

    QRectF widthScreen(QPointF(0, 0), wSize);
    widthScreen.moveCenter(center);

    QRectF r = widthScreen;

    if (options.useFeather) {
        QRectF feather = options.featherRect;
        QSizeF fSize = feather.size();
        fSize.setWidth(fSize.width() * scaleX);
        fSize.setHeight(fSize.height() * scaleY);
        QRectF featherScreen(QPointF(0, 0), fSize);
        featherScreen.moveCenter(center);
        r = r.united(featherScreen);
    }

    if (options.cursorStyle != 0) {
        const qreal iconSize = qMax<qreal>(1.0, options.fixedCursorSize);
        QRectF iconRect(center.x() - iconSize * 0.5, center.y() - iconSize * 0.5, iconSize, iconSize);
        r = r.united(iconRect);
    }

    QRect out = r.toAlignedRect();
    out = out.adjusted(-6, -6, 6, 6);
    return out;
}

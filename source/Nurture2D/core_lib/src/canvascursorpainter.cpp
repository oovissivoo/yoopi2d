/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "canvascursorpainter.h"

#include <QPainter>
#include <QtMath>
#include <QPixmap>
#include <QDebug>

CanvasCursorPainter::CanvasCursorPainter()
{
    setupPen();
    mCachedIconSize = 32.0;
    mCacheValid = true;
    mOutlinedCursorCache.reserve(10);
}

void CanvasCursorPainter::setupPen()
{
	// Black outline pen (3 pixels wide) for visibility on light backgrounds
	mCursorPenOutline = QPen(Qt::black);
	mCursorPenOutline.setWidthF(3.0);
	mCursorPenOutline.setCosmetic(true);

	// White center pen (1 pixel wide) for visibility on dark backgrounds
	mCursorPenCenter = QPen(Qt::white);
	mCursorPenCenter.setWidthF(1.0);
	mCursorPenCenter.setCosmetic(true);
}

void CanvasCursorPainter::paint(QPainter& painter, const QRect& blitRect)
{
    if (mOptions.isAdjusting || mOptions.showCursor) {
        // Track previous rect to ensure old cursor is erased
        mPreviousDirtyRect = mDirtyRect;
        // Reset dirty state at the start of a new paint rhythm; we'll mark dirty again once new rect is computed
        mDirtyRect = QRect();
        mIsDirty = false;
        if (mOptions.cursorStyle == 0) {
            if (mOptions.useFeather) {
                paintFeatherCursor(painter, blitRect, mOptions.widthRect, mOptions.featherRect);
            }
            paintWidthCursor(painter, blitRect, mOptions.widthRect);
        } else {
            if (mOptions.alwaysShowCircle) {
                if (mOptions.useFeather) {
                    paintFeatherCursor(painter, blitRect, mOptions.widthRect, mOptions.featherRect);
                }
                paintWidthCursor(painter, blitRect, mOptions.widthRect);
            }
            paintCustomCursor(painter, blitRect, mOptions.widthRect, mOptions.cursorStyle);
        }
        // Union current and previous rects with conservative padding to avoid trails
        if (!mPreviousDirtyRect.isNull()) {
            mDirtyRect = mDirtyRect.united(mPreviousDirtyRect);
        }
        mDirtyRect = mDirtyRect.adjusted(-3, -3, 3, 3);
        mIsDirty = true;
    }
}

void CanvasCursorPainter::preparePainter(const CanvasCursorPainterOptions& painterOptions, const QTransform& viewTransform)
{
    mOptions = painterOptions;
    // Keep DPR in sync with options (tools may pass it here)
    if (!qFuzzyCompare(mDevicePixelRatio, painterOptions.devicePixelRatio)) {
        setDevicePixelRatio(painterOptions.devicePixelRatio);
    }
    if (mOptions.isAdjusting || mOptions.showCursor) {
        // Compute separate scale factors to better handle non-uniform scaling/shear
        mCurrentScaleX = qSqrt(qPow(viewTransform.m11(), 2) + qPow(viewTransform.m21(), 2));
        mCurrentScaleY = qSqrt(qPow(viewTransform.m12(), 2) + qPow(viewTransform.m22(), 2));

        // Map circle centers fully through the transform
        QPointF newCenter = viewTransform.map(painterOptions.widthRect.center());

        if (painterOptions.useScreenSpace) {
            // CosmicCursorPipeline: keep cursor size constant in screen space, independent of zoom.
            // Width / feather rects are specified in screen pixels; only the center is mapped.
            const qreal size = qMax<qreal>(1.0, painterOptions.screenSpaceSize);
            QSizeF wSize(size, size);
            mOptions.widthRect.setSize(wSize);
            mOptions.widthRect.moveCenter(newCenter);

            if (!painterOptions.featherRect.isNull()) {
                // Preserve relative feather ratio using the original input rects.
                const QSizeF inW = painterOptions.widthRect.size();
                const QSizeF inF = painterOptions.featherRect.size();
                qreal rel = 1.0;
                if (inW.width() > 0.0) {
                    rel = inF.width() / qMax<qreal>(1e-3, inW.width());
                }
                const qreal fSizeW = wSize.width() * rel;
                const qreal fSizeH = wSize.height() * rel;
                QSizeF fSize(fSizeW, fSizeH);
                mOptions.featherRect.setSize(fSize);
                mOptions.featherRect.moveCenter(newCenter);
            } else {
                mOptions.featherRect = QRectF();
            }

            // For screen-space cursors, treat scale as 1.0 so zoom-aware thresholds behave flat.
            mCurrentScaleX = 1.0;
            mCurrentScaleY = 1.0;
        } else {
            // Canvas-space cursor: scale sizes separately on X and Y
            QSizeF wSize = painterOptions.widthRect.size();
            wSize.setWidth(wSize.width() * mCurrentScaleX);
            wSize.setHeight(wSize.height() * mCurrentScaleY);
            mOptions.widthRect.setSize(wSize);
            mOptions.widthRect.moveCenter(newCenter);

            QSizeF fSize = painterOptions.featherRect.size();
            fSize.setWidth(fSize.width() * mCurrentScaleX);
            fSize.setHeight(fSize.height() * mCurrentScaleY);
            mOptions.featherRect.setSize(fSize);
            mOptions.featherRect.moveCenter(newCenter);
        }
    }
}

void CanvasCursorPainter::paintFeatherCursor(QPainter& painter, const QRect& blitRect, const QRectF& widthCircleBounds, const QRectF& featherCircleBounds)
{
    // When the circles are too close to each other, the rendering will appear dotted or almost
    // invisible at certain zoom levels. Use a zoom-aware threshold.
    const qreal currentScale = qMax<qreal>(1.0, qMax(mCurrentScaleX, mCurrentScaleY));
    const qreal minDelta = qMax<qreal>(1.0, 2.0 / currentScale);
    if (widthCircleBounds.width() - featherCircleBounds.width() <= minDelta) {
        return;
    }

    painter.save();

    painter.setClipRect(blitRect);

    // Use SourceOver composition mode instead of XOR
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // First pass: Draw black outline
    painter.setPen(mCursorPenOutline);
    painter.drawEllipse(featherCircleBounds);

    // Second pass: Draw white center on top
    painter.setPen(mCursorPenCenter);
    painter.drawEllipse(featherCircleBounds);

    painter.restore();

    // Expand dirty rect conservatively (union with width bounds too)
    QRectF r = widthCircleBounds.united(featherCircleBounds);
    mDirtyRect = mDirtyRect.united(r.toAlignedRect()).adjusted(-3, -3, 3, 3);
}

void CanvasCursorPainter::paintWidthCursor(QPainter& painter, const QRect& blitRect, const QRectF& widthCircleBounds)
{
    painter.save();

    painter.setClipRect(blitRect);

    // Use SourceOver composition mode instead of XOR for consistent rendering
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // Only draw the cross when the width is bigger than the cross itself
    if (widthCircleBounds.width() > 8) {
        const QPointF& pos = widthCircleBounds.center();
        // Black outline
        painter.setPen(mCursorPenOutline);
        painter.drawLine(QPointF(pos.x() - 2, pos.y()), QPointF(pos.x() + 2, pos.y()));
        painter.drawLine(QPointF(pos.x(), pos.y() - 2), QPointF(pos.x(), pos.y() + 2));
    }

    // First pass: Draw black outline for visibility on light backgrounds
    painter.setPen(mCursorPenOutline);
    painter.drawEllipse(widthCircleBounds);

    // Second pass: Draw white center on top for visibility on dark backgrounds
    painter.setPen(mCursorPenCenter);
    painter.drawEllipse(widthCircleBounds);

    // Draw center cross with white center
    if (widthCircleBounds.width() > 8) {
        const QPointF& pos = widthCircleBounds.center();
        painter.drawLine(QPointF(pos.x() - 2, pos.y()), QPointF(pos.x() + 2, pos.y()));
        painter.drawLine(QPointF(pos.x(), pos.y() - 2), QPointF(pos.x(), pos.y() + 2));
    }
    painter.restore();

    mDirtyRect = mDirtyRect.united(widthCircleBounds.toAlignedRect()).adjusted(-3, -3, 3, 3);
}

void CanvasCursorPainter::clearDirty()
{
    mDirtyRect = QRect();
    mPreviousDirtyRect = QRect();
    mIsDirty = false;
}

QPixmap CanvasCursorPainter::getOrCreateOutlinedIcon(int cursorStyle, qreal iconSize)
{
    // Scale icon by device pixel ratio for crisp rendering
    const qreal scaledSize = iconSize * qMax<qreal>(1.0, mDevicePixelRatio);

    if (!qFuzzyCompare(mCachedIconSize, scaledSize)) {
        invalidateIconCache();
        mCachedIconSize = scaledSize;
    }

    if (mOutlinedCursorCache.contains(cursorStyle)) {
        return mOutlinedCursorCache[cursorStyle];
    }

    QString iconPath;
    switch (cursorStyle) {
        case 1: iconPath = ":/CustomCursor/Cross.svg"; break;
        case 2: iconPath = ":/CustomCursor/triUp.svg"; break;
        case 3: iconPath = ":/CustomCursor/triDown.svg"; break;
        case 4: iconPath = ":/CustomCursor/triLeft.svg"; break;
        case 5: iconPath = ":/CustomCursor/triRight.svg"; break;
        case 6: iconPath = ":/CustomCursor/triLeftUp.svg"; break;
        case 7: iconPath = ":/CustomCursor/triLeftDown.svg"; break;
        case 8: iconPath = ":/CustomCursor/triRightUp.svg"; break;
        case 9: iconPath = ":/CustomCursor/triRightDown.svg"; break;
        default: iconPath = ":/icons/general/cross.png"; break;
    }

    QPixmap baseIcon(iconPath);
    if (baseIcon.isNull()) {
        qWarning() << "CanvasCursorPainter: failed to load cursor icon" << iconPath;
        // Fallback to default cross icon
        baseIcon = QPixmap(":/icons/general/cross.png");
        if (baseIcon.isNull()) {
            qCritical() << "CanvasCursorPainter: failed to load fallback cross icon";
            return QPixmap();
        }
    }

    QPixmap scaledIcon = baseIcon.scaled(scaledSize, scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaledIcon.setDevicePixelRatio(mDevicePixelRatio);

    QPixmap outlinedIcon = createOutlinedIcon(scaledIcon);
    outlinedIcon.setDevicePixelRatio(mDevicePixelRatio);
    mOutlinedCursorCache[cursorStyle] = outlinedIcon;
    return outlinedIcon;
}

QPixmap CanvasCursorPainter::createOutlinedIcon(const QPixmap& baseIcon)
{
    const int offset = qCeil(2 * qMax<qreal>(1.0, mDevicePixelRatio));
    QPixmap outlinedIcon(baseIcon.size() + QSize(offset * 2, offset * 2));
    outlinedIcon.fill(Qt::transparent);

    QPainter outlinePainter(&outlinedIcon);
    outlinePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    QPixmap blackIcon = baseIcon;
    QPainter blackPainter(&blackIcon);
    blackPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    blackPainter.fillRect(blackIcon.rect(), Qt::black);
    blackPainter.end();

    const int delta = qMax(1, qCeil(mDevicePixelRatio));
    for (int dx = -delta; dx <= delta; dx++) {
        for (int dy = -delta; dy <= delta; dy++) {
            if (dx != 0 || dy != 0) {
                outlinePainter.drawPixmap(offset + dx, offset + dy, blackIcon);
            }
        }
    }

    outlinePainter.drawPixmap(offset, offset, baseIcon);
    outlinePainter.end();

    return outlinedIcon;
}

void CanvasCursorPainter::invalidateIconCache()
{
    mOutlinedCursorCache.clear();
    mCacheValid = false;
}

void CanvasCursorPainter::paintCustomCursor(QPainter& painter, const QRect& blitRect, const QRectF& widthCircleBounds, int cursorStyle)
{
    painter.save();
    painter.setClipRect(blitRect);

    const qreal baseIconSize = mOptions.fixedCursorSize;
    QPixmap outlinedIcon = getOrCreateOutlinedIcon(cursorStyle, baseIconSize);

    if (outlinedIcon.isNull()) {
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        const QPointF& pos = widthCircleBounds.center();
        painter.setPen(mCursorPenOutline);
        painter.drawLine(QPointF(pos.x() - 5, pos.y()), QPointF(pos.x() + 5, pos.y()));
        painter.drawLine(QPointF(pos.x(), pos.y() - 5), QPointF(pos.x(), pos.y() + 5));
        painter.setPen(mCursorPenCenter);
        painter.drawLine(QPointF(pos.x() - 5, pos.y()), QPointF(pos.x() + 5, pos.y()));
        painter.drawLine(QPointF(pos.x(), pos.y() - 5), QPointF(pos.x(), pos.y() + 5));
        painter.restore();
        mDirtyRect = mDirtyRect.united(widthCircleBounds.toAlignedRect()).adjusted(-3, -3, 3, 3);
        return;
    }

    const QPointF center = widthCircleBounds.center();
    // Triangle cursors (styles 2-9) were authored to occupy roughly 50% of the
    // available icon space, so render them at 2x the nominal size to visually
    // match the other cursor shapes.
    const bool isTriangleStyle = (cursorStyle >= 2 && cursorStyle <= 9);
    const qreal scale = isTriangleStyle ? 2.0 : 1.0;
    const qreal baseDrawSize = qMax<qreal>(1.0, qMax(widthCircleBounds.width(), widthCircleBounds.height()));
    const qreal drawSize = baseDrawSize * scale;
    QRectF target(center.x() - drawSize * 0.5, center.y() - drawSize * 0.5, drawSize, drawSize);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawPixmap(target, outlinedIcon, outlinedIcon.rect());
    painter.restore();
    mDirtyRect = mDirtyRect.united(target.united(widthCircleBounds).toAlignedRect()).adjusted(-3, -3, 3, 3);
}

void CanvasCursorPainter::setIconSize(qreal size)
{
    if (!qFuzzyCompare(mCachedIconSize, size)) {
        mCachedIconSize = size;
        invalidateIconCache();
    }
}

void CanvasCursorPainter::clearCache()
{
    invalidateIconCache();
}

int CanvasCursorPainter::getCacheSize() const
{
    return mOutlinedCursorCache.size();
}

void CanvasCursorPainter::setDevicePixelRatio(qreal dpr)
{
    const qreal newDpr = (dpr > 0.0 ? dpr : 1.0);
    if (!qFuzzyCompare(mDevicePixelRatio, newDpr)) {
        mDevicePixelRatio = newDpr;
        invalidateIconCache();
    }
}

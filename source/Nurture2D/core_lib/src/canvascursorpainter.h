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
#ifndef CANVASCURSORPAINTER_H
#define CANVASCURSORPAINTER_H

#include <QPen>
#include <QHash>
#include <QPixmap>

class QPainter;

struct CanvasCursorPainterOptions
{
    QRectF widthRect;
    QRectF featherRect;
    bool isAdjusting = false;
    bool useFeather = false;
    bool showCursor = false;
    int cursorStyle = 0; // 0=Circle, 1=Cross, 2-9=Triangle variants
    bool alwaysShowCircle = false;
    qreal fixedCursorSize = 32.0;
    // When true, widthRect / featherRect are interpreted in screen-space pixels
    // and will NOT be scaled by the view transform. Used by the CosmicCursorPipeline
    // so cursor size is independent of zoom.
    bool useScreenSpace = false;
    qreal screenSpaceSize = 20.0;
    // Device pixel ratio for high-DPI displays (1.0 = default)
    qreal devicePixelRatio = 1.0;
};

class CanvasCursorPainter
{

public:
    CanvasCursorPainter();
    void paint(QPainter& painter, const QRect& blitRect);

    void preparePainter(const CanvasCursorPainterOptions& painterOptions, const QTransform& viewTransform);

    const QRect dirtyRect() { return mDirtyRect; }
    bool isDirty() const { return mIsDirty; }
    void clearDirty();

    // Cache management API
    void setIconSize(qreal size);
    void clearCache();
    int getCacheSize() const;

    /**
     * Set the current device pixel ratio used for icon rendering.
     * Changing this value invalidates the outlined icon cache.
     */
    void setDevicePixelRatio(qreal dpr);

private:

    void setupPen();

    /// @brief precision circular cursor: used for drawing a cursor on the canvas.
    void paintWidthCursor(QPainter& painter, const QRect& blitRect, const QRectF& widthCircleBounds);
    void paintFeatherCursor(QPainter& painter, const QRect& blitRect, const QRectF& widthCircleBounds, const QRectF& featherCircleBounds);
    void paintCustomCursor(QPainter& painter, const QRect& blitRect, const QRectF& widthCircleBounds, int cursorStyle);

    // Cached custom cursor helpers
    QPixmap getOrCreateOutlinedIcon(int cursorStyle, qreal iconSize);
    void invalidateIconCache();
    QPixmap createOutlinedIcon(const QPixmap& baseIcon);

    CanvasCursorPainterOptions mOptions;
    QRect mDirtyRect;
    QRect mPreviousDirtyRect; // previous cursor area for conservative updates
    bool mIsDirty = false;

    QPen mCursorPenOutline;  // Black outline for visibility on light backgrounds
    QPen mCursorPenCenter;   // White center for visibility on dark backgrounds
    QHash<int, QPixmap> mOutlinedCursorCache; // fully prepared outlined icons
    qreal mCachedIconSize = 32.0;
    bool mCacheValid = true; // Tracks cache validity for outlined icons

    // HiDPI handling
    qreal mDevicePixelRatio = 1.0;
    // Cached view scale components (used for zoom-aware thresholds)
    qreal mCurrentScaleX = 1.0;
    qreal mCurrentScaleY = 1.0;
};

#endif // CANVASCURSORPAINTER_H

#include "dplatetool.h"

#include <QSettings>
#include "managers/settingsmanager.h"
#include <QPixmap>
#include <QPen>
 #include <array>
 #include <limits>

#include "editor.h"
#include "scribblearea.h"
#include "pointerevent.h"
#include "layermanager.h"
#include "structure/layer.h"
#include "managers/viewmanager.h"
#include "managers/colormanager.h"

 static std::array<QPointF, 4> dplateLineAnchorPoints(const QPointF& tl, qreal u, int bigCount, int smallCount)
 {
     // Big boxes are 2u wide × u tall, stacked downward from tl.
     // Small boxes are u wide × u/2 tall, stacked upward-left from tl.
     const qreal bw = u * 2.0;
     const qreal sw = u;
     const qreal sh = u * 0.5;

     // Bottom-left of the bottom-most big box
     const QPointF bigBottomLeft  = tl + QPointF(0,   bigCount * u);
     // Bottom-right of the bottom-most big box
     const QPointF bigBottomRight = tl + QPointF(bw,  bigCount * u);
     // Top-right of the top-most big box
     const QPointF bigTopRight    = tl + QPointF(bw,  0);
     // Top-left of the top-most small box
     const QPointF smallTopLeft   = tl + QPointF(-sw, -smallCount * sh);

     return { bigBottomLeft, bigBottomRight, bigTopRight, smallTopLeft };
 }

 static QPointF nearestPoint(const QPointF& ref, const std::array<QPointF, 4>& candidates)
 {
     qreal bestDist2 = std::numeric_limits<qreal>::infinity();
     QPointF best = candidates[0];
     for (const QPointF& p : candidates) {
         const QPointF d = p - ref;
         const qreal dist2 = d.x() * d.x() + d.y() * d.y();
         if (dist2 < bestDist2) {
             bestDist2 = dist2;
             best = p;
         }
     }
     return best;
 }

DplateTool::DplateTool(QObject* parent) : BaseTool(parent) {}

void DplateTool::loadSettings()
{
    // Disable width UI for Dplate; use fixed unit
    mPropertyEnabled[WIDTH] = false;
    mPropertyEnabled[ANTI_ALIASING] = false; // AA not used
    mPropertyEnabled[DRAW_BEHIND] = true;

    // Fixed default shape unit
    properties.width = 20.0;
    SettingsManager* s = SettingsManager::instance();
    // Draw Behind is global; keep legacy read for migration but actual value comes from manager
    properties.drawBehind = s->getValue("dplateDrawBehind", false).toBool();

    // Box counts
    mBigBoxCount   = qBound(0, s->getValue("dplateBigBoxCount",   3).toInt(), 3);
    mSmallBoxCount = qBound(0, s->getValue("dplateSmallBoxCount", 0).toInt(), 3);
}

void DplateTool::saveSettings()
{
    // Persist fixed width for completeness
    SettingsManager* s = SettingsManager::instance();
    s->deferredSetValue("dplateUnit", properties.width);
    // Draw Behind is global; do not persist per-tool anymore (compat no-op)
    s->deferredSetValue("dplateDrawBehind", properties.drawBehind);
    // Box counts
    s->deferredSetValue("dplateBigBoxCount",   mBigBoxCount);
    s->deferredSetValue("dplateSmallBoxCount", mSmallBoxCount);
    clearSettingsDirty();
}

QCursor DplateTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void DplateTool::resetToDefault()
{
    setWidth(20.0);
    setBigBoxCount(3);
    setSmallBoxCount(0);
}

void DplateTool::setWidth(const qreal /*width*/)
{
    // Ignore external width changes; enforce fixed unit
    properties.width = 20.0;
}

QPainterPath DplateTool::buildShapePath(const QPointF& tl, qreal u) const
{
    QPainterPath path;

    // Big boxes: 2u wide × u tall rectangles, stacked downward from tl
    const qreal bw = u * 2.0;
    for (int i = 0; i < mBigBoxCount; ++i) {
        path.addRect(QRectF(tl + QPointF(0, i * u), QSizeF(bw, u)));
    }

    // Small boxes: u wide × u/2 tall rectangles, stacked upward from tl.
    // Bottom-right of the bottom-most small box aligns with tl.
    const qreal sw = u;
    const qreal sh = u * 0.5;
    for (int i = 0; i < mSmallBoxCount; ++i) {
        const QPointF origin = tl + QPointF(-sw, -(i + 1) * sh);
        path.addRect(QRectF(origin, QSizeF(sw, sh)));
    }

    return path;
}

void DplateTool::drawPreviewAt(const QPointF& tl, qreal u)
{
    // Clear the buffer for a fresh preview draw
    mScribbleArea->mTiledBuffer.clear();

    const qreal strokeWidth = 1.0;
    const QColor activeColor = mEditor ? mEditor->color()->frontColor() : mStrokeColor;
    QPen pen(activeColor, strokeWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);

    // Draw the drag line first so it appears behind the boxes
    if (mDragging) {
        QPainterPath linePath;
        linePath.moveTo(mStartPoint);
        const QPointF end = nearestPoint(mStartPoint, dplateLineAnchorPoints(tl, u, mBigBoxCount, mSmallBoxCount));
        linePath.lineTo(end);
        // Use DestinationOver to ensure the line is composited behind any existing pixels
        mScribbleArea->mTiledBuffer.drawPath(linePath, pen, Qt::NoBrush, QPainter::CompositionMode_DestinationOver, false);
    }

    // Draw the boxes last so they appear above the drag line
    QPainterPath shape = buildShapePath(tl, u);
    mScribbleArea->mTiledBuffer.drawPath(shape, pen, QBrush(mFillColor, Qt::SolidPattern), QPainter::CompositionMode_SourceOver, false);

    // Compute canvas-bounds of current preview
    QRectF newCanvasBounds = mScribbleArea->mTiledBuffer.bounds();

    // Compute screen update rect that covers both old and new preview bounds
    QRect updateRect;
    if (mHasLastPreviewBounds) {
        QRectF unionCanvas = newCanvasBounds.united(mLastPreviewCanvasBounds);
        updateRect = mScribbleArea->editor()->view()->mapCanvasToScreen(unionCanvas).toRect().adjusted(-1, -1, 1, 1);
    } else {
        updateRect = mScribbleArea->editor()->view()->mapCanvasToScreen(newCanvasBounds).toRect().adjusted(-1, -1, 1, 1);
        mHasLastPreviewBounds = true;
    }
    mLastPreviewCanvasBounds = newCanvasBounds;

    mScribbleArea->update(updateRect);
}

void DplateTool::pointerPressEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }

    auto* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) {
        return;
    }

    mScribbleArea->handleDrawingOnEmptyFrame();

    // Prepare undo: capture state (backup removed)
    mUndoSaveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);

    mStartPoint = event->canvasPos();
    mDragging = true;
    // At press: only memorize start, wait for move to show line + shape at cursor

    BaseTool::pointerPressEvent(event);
}

void DplateTool::pointerMoveEvent(PointerEvent* event)
{
    if (!mDragging) { return; }
    drawPreviewAt(event->canvasPos(), properties.width);
    BaseTool::pointerMoveEvent(event);
}

void DplateTool::pointerReleaseEvent(PointerEvent* event)
{
    if (!mDragging) { return; }
    QPointF tl = event->canvasPos();

    // Draw both shape and line into buffer before committing
    mDragging = true;
    drawPreviewAt(tl, properties.width);
    mDragging = false;

    // Commit preview buffer to layer
    mScribbleArea->endStroke();

    // Clear preview state and request repaint of last preview area
    mScribbleArea->mTiledBuffer.clear();
    if (mHasLastPreviewBounds) {
        QRect updateRect = mScribbleArea->editor()->view()->mapCanvasToScreen(mLastPreviewCanvasBounds).toRect().adjusted(-1, -1, 1, 1);
        mScribbleArea->update(updateRect);
    } else {
        mScribbleArea->update();
    }
    mHasLastPreviewBounds = false;
    mLastPreviewCanvasBounds = QRectF();

    // Finalize undo record so Ctrl+Z/Ctrl+Y affects this stroke
    if (mUndoSaveState && mEditor && mEditor->undoRedo()) {
        mEditor->undoRedo()->record(mUndoSaveState, typeName());
        mUndoSaveState = nullptr;
    }
    BaseTool::pointerReleaseEvent(event);
}

void DplateTool::showCenterPreview(qreal unit)
{
    QPointF center = mScribbleArea->getCentralPoint();
    mPreviewActive = true;
    drawPreviewAt(center, unit);
}

void DplateTool::clearPreview()
{
    mPreviewActive = false;
    mScribbleArea->clearDrawingBuffer();
    if (mHasLastPreviewBounds) {
        QRect updateRect = mScribbleArea->editor()->view()->mapCanvasToScreen(mLastPreviewCanvasBounds).toRect().adjusted(-1, -1, 1, 1);
        mScribbleArea->update(updateRect);
    } else {
        mScribbleArea->update();
    }
    mHasLastPreviewBounds = false;
    mLastPreviewCanvasBounds = QRectF();
}

void DplateTool::setBigBoxCount(int count)
{
    mBigBoxCount = qBound(0, count, 3);
    markSettingsDirty();
}

void DplateTool::setSmallBoxCount(int count)
{
    mSmallBoxCount = qBound(0, count, 3);
    markSettingsDirty();
} 
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
#include "buckettool.h"

#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QSet>
#include "managers/settingsmanager.h"
#include "pointerevent.h"
#include "../../../CUI/cosmicuimetrics.h"

#include "layer.h"
#include "layervector.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "layercamera.h"
#include "layermanager.h"
#include "colormanager.h"
#include "viewmanager.h"
#include "vectorimage.h"
#include "editor.h"
#include "scribblearea.h"


BucketTool::BucketTool(QObject* parent) : StrokeTool(parent)
{
}

ToolType BucketTool::type()
{
    return BUCKET;
}

void BucketTool::loadSettings()
{
    mPropertyEnabled[TOLERANCE] = true;
    mPropertyEnabled[WIDTH] = true;
    mPropertyEnabled[FILL_MODE] = true;

    SettingsManager* settings = SettingsManager::instance();

    properties.width = settings->getValue("fillThickness", 4.0).toDouble();
    properties.feather = 10;
    properties.stabilizerLevel = StabilizationLevel::NONE;
    properties.useAA = DISABLED;
    // Force color tolerance off and zero
    properties.tolerance = 0.0;
    properties.toleranceEnabled = false;

    // Expand Fill removed: force defaults to zero/false regardless of saved settings
    properties.bucketFillExpand = 0;
    properties.bucketFillExpandEnabled = false;
    properties.bucketFillReferenceMode = settings->getValue(SETTING_BUCKET_FILL_REFERENCE_MODE, 0).toInt();

    // Bucket blend mode: always use Replace for consistency with hidden UI
    properties.fillMode = 1; // 0=Overlay, 1=Replace, 2=Behind
    properties.colorReplaceMode = settings->getValue("ColorReplaceMode", false).toBool();
    properties.bucketSameColorOnly = settings->getValue("BucketSameColorOnly", false).toBool();
    properties.bucketMultiKeyframeFill = settings->getValue("BucketMultiKeyframeFill", false).toBool();

    // Gap fill: hardcode to Off for bitmap bucket; keep 0 regardless of saved value
    properties.bucketGapFillMode = 0;
}

void BucketTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();

    settings->deferredSetValue("fillThickness", properties.width);
    // Persist fixed zero tolerance for consistency
    settings->deferredSetValue(SETTING_BUCKET_TOLERANCE, 0.0);
    settings->deferredSetValue(SETTING_BUCKET_TOLERANCE_ON, false);
    // Expand Fill removed: do not persist these settings anymore
    settings->deferredSetValue(SETTING_BUCKET_FILL_REFERENCE_MODE, properties.bucketFillReferenceMode);
    settings->deferredSetValue(SETTING_FILL_MODE, properties.fillMode);
    settings->deferredSetValue("ColorReplaceMode", properties.colorReplaceMode);
    settings->deferredSetValue("BucketSameColorOnly", properties.bucketSameColorOnly);
    settings->deferredSetValue("BucketMultiKeyframeFill", properties.bucketMultiKeyframeFill);
    settings->deferredSetValue(SETTING_BUCKET_GAP_FILL_MODE, properties.bucketGapFillMode);
    clearSettingsDirty();
}

void BucketTool::resetToDefault()
{
    setWidth(4.0);
    setTolerance(32.0);
    setFillMode(0);
    // Expand Fill removed
    setToleranceEnabled(false);
    setFillReferenceMode(0);
}

QCursor BucketTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":icons/general/cursor-bucket.svg"), -1, 17);
    }
    else
    {
        return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
    }
}

void BucketTool::setTolerance(const int tolerance)
{
    if (!qFuzzyCompare(properties.tolerance, qreal(tolerance))) { properties.tolerance = tolerance; markSettingsDirty(); }
}

/**
 * @brief BrushTool::setWidth
 * @param width
 * set fill thickness
 */
void BucketTool::setWidth(const qreal width)
{
    if (!qFuzzyCompare(properties.width, width)) { properties.width = width; markSettingsDirty(); }
}

void BucketTool::setFillMode(int mode)
{
    if (properties.fillMode != mode) { properties.fillMode = mode; markSettingsDirty(); }
}

void BucketTool::setToleranceEnabled(const bool enabled)
{
    if (properties.toleranceEnabled != enabled) { properties.toleranceEnabled = enabled; markSettingsDirty(); }

}

void BucketTool::setFillExpandEnabled(const bool enabled)
{
    if (properties.bucketFillExpandEnabled != enabled) { properties.bucketFillExpandEnabled = enabled; markSettingsDirty(); }

}

void BucketTool::setFillExpand(const int fillExpandValue)
{
    if (properties.bucketFillExpand != fillExpandValue) { properties.bucketFillExpand = fillExpandValue; markSettingsDirty(); }
}

void BucketTool::setFillReferenceMode(int referenceMode)
{
    if (properties.bucketFillReferenceMode != referenceMode) { properties.bucketFillReferenceMode = referenceMode; markSettingsDirty(); }
}

void BucketTool::pointerPressEvent(PointerEvent* event)
{
    // Ctrl-modified temporary stroke mode using StrokeTool infrastructure
    mCtrlStrokeActive = (event->modifiers().testFlag(Qt::ControlModifier) && event->button() == Qt::LeftButton);
    if (mCtrlStrokeActive)
    {
        mInterpolator.pointerPressEvent(event);
        if (handleQuickSizing(event)) {
            return;
        }

        // Initialize stroke state using base class infrastructure
        mCtrlMouseDownPoint = getCurrentPoint();
        mCtrlLastBrushPoint = getCurrentPoint();
        mCtrlCurrentPressure = 0.5; // Fixed pressure for Ctrl mode
        mCtrlStrokeHybridPoints.clear();
        mCtrlStrokeHybridPoints.append(mCtrlMouseDownPoint);

        // Force draw-behind for this temporary stroke
        mCtrlPrevDrawBehind = properties.drawBehind;
        properties.drawBehind = true;

        // Start stroke and call through base for consistent setup
        startStroke(event->inputType());
        StrokeTool::pointerPressEvent(event);

        // BEGIN: Initialize undo macro for Ctrl-stroke at press so it groups the entire stroke
        if (mEditor && mEditor->undoRedo()) {
            mUndoSaveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
            mEditor->undoRedo()->undoStack()->beginMacro(typeName());
        }

        return; // skip normal bucket setup
    }
    // Multi-Keyframe Fill gizmo behavior: handle before stroke init to avoid inconsistent state
    if (properties.bucketMultiKeyframeFill && event->button() == Qt::LeftButton)
    {
        QPointF screenPos = mEditor->view()->mapCanvasToScreen(event->canvasPos());
        if (!mGizmoActive)
        {
            // First click: create gizmo at clicked position
            mGizmoActive = true;
            mGizmoDragging = false;
            mGizmoStartFrame = mEditor->currentFrame();
            mGizmoCanvasPos = event->canvasPos();
            mScribbleArea->update();
            // Suppress normal press/release handling for this click
            mSuppressReleaseOnce = true;
            return;
        }
        else
        {
            if (hitGizmoScreen(screenPos))
            {
                // Begin dragging gizmo only if left button is pressed
                if (event->buttons() & Qt::LeftButton) {
                    mGizmoDragging = true;
                    mSuppressReleaseOnce = true;
                    return;
                }
            }
            else
            {
                // Clicked elsewhere: perform range fill and clear gizmo, suppress stroke
                performRangeFillFromGizmo(mEditor->currentFrame());
                mGizmoActive = false;
                mGizmoDragging = false;
                mGizmoStartFrame = -1;
                mScribbleArea->update();
                mSuppressReleaseOnce = true;
                return;
            }
        }
    }

    mInterpolator.pointerPressEvent(event);
    startStroke(event->inputType());

    // BEGIN: Continuous fill grouping - only for Bitmap layer fills
    if (!mCtrlStrokeActive && !mGizmoActive && event->button() == Qt::LeftButton && mEditor && mEditor->undoRedo()) {
        Layer* currLayerForMacro = mEditor->layers()->currentLayer();
        if (currLayerForMacro && currLayerForMacro->type() == Layer::BITMAP) {
            mContinuousFillMacroActive = true;
            mContinuousFillUndoState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
            mEditor->undoRedo()->undoStack()->beginMacro(tr("Bucket Fill"));
        }
    }

    Layer* targetLayer = mEditor->layers()->currentLayer();

    if (targetLayer->type() != Layer::BITMAP) { return; }

    LayerCamera* layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());

    mBitmapBucket = BitmapBucket(mEditor,
                                 mEditor->color()->frontColor(),
                                 layerCam ? layerCam->getViewAtFrame(mEditor->currentFrame()).inverted().mapRect(layerCam->getViewRect()) : QRect(),
                                 getCurrentPoint(),
                                 properties,
                                 mEditor->autoLineFillEnabled(),
                                 mEditor->autoLineColors(),
                                 mEditor->ignoreFillColor(),
                                 mEditor->ignoreFillEnabled());

    // Because we can change layer to on the fly, but we do not act reactively
    // on it, it's necessary to invalidate layer cache on press event.
    // Otherwise, the cache will be drawn until a move event has been initiated.
    mScribbleArea->invalidatePainterCaches();

    // Initialize drag preview state
    mStartPoint = event->canvasPos();
    mDragging = true;
    mDragBufferHasStart = false;
    mDragOverlayBuffer = QImage(); // will be allocated on first move
    mDragLastScreenPoint = QPointF();
    mDragStartScreenPoint = QPointF();

    // Enable gap debug overlay for bitmap bucket fills while LMB is held down
    if (properties.bucketGapFillMode > 0 && event->button() == Qt::LeftButton) {
        mGapDebugOverlayActive = true;
        mGapDebugClickPoint = event->canvasPos();
        if (mScribbleArea) {
            mScribbleArea->update();
        }
    }
}

void BucketTool::pointerMoveEvent(PointerEvent* event)
{
    // Ctrl temporary stroke drawing using PenTool-like pipeline
    if (mCtrlStrokeActive && (event->buttons() & Qt::LeftButton))
    {
        mInterpolator.pointerMoveEvent(event);
        if (handleQuickSizing(event)) {
            return;
        }

        // Draw stroke using PenTool's proven logic
        drawCtrlStroke();

        // Update StrokeTool state
        StrokeTool::pointerMoveEvent(event);

        return; // bypass bucket behavior while Ctrl-drawing
    }

    mInterpolator.pointerMoveEvent(event);
    // If dragging gizmo with left button held, update its position in canvas space and repaint; suppress normal fill/preview
    if (properties.bucketMultiKeyframeFill && mGizmoActive && mGizmoDragging && (event->buttons() & Qt::LeftButton))
    {
        mGizmoCanvasPos = event->canvasPos();
        mScribbleArea->update();
        return;
    }
    if (properties.bucketMultiKeyframeFill && mSuppressReleaseOnce) {
        // While suppressing this gesture (gizmo), do not process normal drag behavior
        return;
    }
    if (event->buttons() & Qt::LeftButton && event->inputType() == mCurrentInputType)
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer->type() == Layer::VECTOR)
        {
            drawStroke();
        }
        else if (layer->type() == Layer::BITMAP)
        {
            paintBitmap();
            mFilledOnMove = true;
        }
        // Keep debug click point in sync with current fill point while dragging
        if (mGapDebugOverlayActive && layer && layer->type() == Layer::BITMAP) {
            mGapDebugClickPoint = event->canvasPos();
            if (mScribbleArea) {
                mScribbleArea->update();
            }
        }
        // Draw drag preview line regardless of layer type when dragging
        if (mDragging) {
            drawDragPreviewAt(event->canvasPos());
        }
    }
}

void BucketTool::pointerReleaseEvent(PointerEvent* event)
{
    // End Ctrl stroke using StrokeTool infrastructure
    if (mCtrlStrokeActive && event->button() == Qt::LeftButton)
    {
        mInterpolator.pointerReleaseEvent(event);
        if (handleQuickSizing(event)) {
            return;
        }

        if (event->inputType() != mCurrentInputType) return;

        // NOTE: Ctrl-stroke macro is now started in pointerPressEvent() where stroke begins.

        // Handle tap vs stroke
        qreal distance = QLineF(getCurrentPoint(), mCtrlMouseDownPoint).length();
        if (distance < 1)
        {
            const qreal brushWidth = 2.0;
            const QColor strokeColor = mEditor->color()->frontColor();
            mScribbleArea->drawPen(mCtrlMouseDownPoint, brushWidth, strokeColor, /*useAA*/ false);
            if (mCtrlStrokeHybridPoints.isEmpty()) {
                mCtrlStrokeHybridPoints.append(mCtrlMouseDownPoint);
            }
        }
        else
        {
            // Ensure final segment is drawn
            drawCtrlStroke();
        }

        // Commit tiled buffer to bitmap
        mScribbleArea->paintBitmapBuffer();

        // Record hybrid centerline for Ctrl stroke on bitmap layers so gap-fill can use it
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer && layer->type() == Layer::BITMAP)
        {
            auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
            KeyFrame* _kf = bitmapLayer->getLastKeyFrameAtPosition(mEditor->currentFrame());
            BitmapKeyFrame* _bkf = _kf ? static_cast<BitmapKeyFrame*>(_kf) : nullptr;
            BitmapImage* cellImg = _bkf ? bitmapLayer->cellBank()->getActiveInnerLayerImage(_bkf->cellId()) : nullptr;
            if (cellImg && mCtrlStrokeHybridPoints.size() >= 2)
            {
                QVector<QPointF> pts = QVector<QPointF>::fromList(mCtrlStrokeHybridPoints.toList());
                const qreal brushWidth = 2.0;
                cellImg->addHybridStroke(pts, brushWidth);
            }
        }

        // Record undo and end macro for Ctrl-stroke
        if (mEditor && mEditor->undoRedo() && mUndoSaveState) {
            mEditor->undoRedo()->record(mUndoSaveState, typeName());
            mUndoSaveState = nullptr;
        }
        if (mEditor && mEditor->undoRedo()) {
            mEditor->undoRedo()->undoStack()->endMacro();
        }

        // End stroke
        // Suppress base-class auto-recording; BucketTool manages its own undo recording
        // via mContinuousFillUndoState or explicit records in the vector path above.
        mUndoSaveState = nullptr;
        endStroke();

        // Restore draw-behind property
        properties.drawBehind = mCtrlPrevDrawBehind;
        mCtrlPrevDrawBehind = false;

        // Clear Ctrl mode state
        mCtrlStrokeActive = false;
        mCtrlMouseDownPoint = QPointF();
        mCtrlLastBrushPoint = QPointF();
        mCtrlStrokeHybridPoints.clear();

        StrokeTool::pointerReleaseEvent(event);

        // Disable gap debug overlay at end of Ctrl-stroke
        mGapDebugOverlayActive = false;
        if (mScribbleArea) {
            mScribbleArea->update();
        }

        return; // skip normal bucket release handling
    }

    mInterpolator.pointerReleaseEvent(event);
    if (event->inputType() != mCurrentInputType) return;

    Layer* layer = editor()->layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (event->button() == Qt::LeftButton)
    {
        // If we were dragging the gizmo, stop dragging and do nothing else
        if (properties.bucketMultiKeyframeFill && mGizmoActive && mGizmoDragging)
        {
            mGizmoDragging = false;
            // This press/release was owned by gizmo; suppress endStroke
            mSuppressReleaseOnce = false;
            return;
        }
        if (properties.bucketMultiKeyframeFill && mSuppressReleaseOnce) {
            // Swallow this release for gizmo actions
            mSuppressReleaseOnce = false;
            return;
        }
        // For vector fills: capture state, paint, then record undo
        if (layer->type() == Layer::VECTOR) {
            const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
            paintVector(layer);
            if (mEditor && mEditor->undoRedo()) mEditor->undoRedo()->record(saveState, typeName());
            // Prevent duplicate recording at macro close for vector fills
            // (vector path already recorded explicitly above)
            mContinuousFillUndoState = nullptr;
        }
        else if (layer->type() == Layer::BITMAP && !mFilledOnMove)
        {
            paintBitmap();
        }
    }
    mFilledOnMove = false;

    // Clear drag preview BEFORE ending stroke to avoid committing preview to layer
    if (mDragging) {
        mDragging = false;
        mDragOverlayBuffer = QImage();
        mDragBufferHasStart = false;
        mScribbleArea->update();
    }

    // Prevent base-class endStroke() from recording another undo snapshot; BucketTool already recorded appropriately
    mUndoSaveState = nullptr;
    endStroke();

    // Disable gap debug overlay when the bucket stroke is finished
    if (event->button() == Qt::LeftButton) {
        mGapDebugOverlayActive = false;
        if (mScribbleArea) {
            mScribbleArea->update();
        }
    }

    // If we started a continuous fill macro on press, close it here by recording final state and ending macro
    if (mContinuousFillMacroActive && mEditor && mEditor->undoRedo()) {
        if (mContinuousFillUndoState) {
            mEditor->undoRedo()->record(mContinuousFillUndoState, tr("Bucket Fill"));
            mContinuousFillUndoState = nullptr;
        }
        mEditor->undoRedo()->undoStack()->endMacro();
        mContinuousFillMacroActive = false;
    }
}

void BucketTool::paint(QPainter& painter, const QRect& blitRect)
{
    Q_UNUSED(blitRect);
    if (properties.bucketMultiKeyframeFill && mGizmoActive)
    {
        drawGizmo(painter);
    }

    // Draw optional gap-fill debug overlay for bitmap bucket tool
    drawGapDebugOverlay(painter);

    // Draw drag preview overlay (screen space, blit raster buffer)
    if (mDragging && !mDragOverlayBuffer.isNull()) {
        drawDragPreviewOverlay(painter);
    }
}

void BucketTool::paintBitmap()
{
    mBitmapBucket.paint(getCurrentPoint(), [this](BucketState progress, int layerIndex, int frameIndex)
    {
        if (progress == BucketState::WillFillTarget)
        {
            // During continuous drag fills we avoid creating per-fill undo entries.
            // If continuous fill macro is NOT active (single click), record immediately.
            if (!mContinuousFillMacroActive) {
                const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
                mEditor->undoRedo()->record(saveState, typeName());
            }
        }
        else if (progress == BucketState::DidFillTarget)
        {
            mEditor->setModified(layerIndex, frameIndex);
        }
    });
}

void BucketTool::paintVector(Layer* layer)
{
    mScribbleArea->clearDrawingBuffer();

    VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame(), 0);
    if (vectorImage == nullptr) { return; } // Can happen if the first frame is deleted while drawing

    if (!vectorImage->isPathFilled())
    {
        vectorImage->fillSelectedPath(mEditor->color()->frontColorNumber());
    }

    vectorImage->applyWidthToSelection(properties.width);
    vectorImage->applyColorToSelectedCurve(mEditor->color()->frontColorNumber());
    vectorImage->applyColorToSelectedArea(mEditor->color()->frontColorNumber());

    applyChanges();

    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
}

void BucketTool::applyChanges()
{
    mScribbleArea->applyTransformedSelection();
}

void BucketTool::drawStroke()
{
    StrokeTool::drawStroke();

    if (properties.stabilizerLevel != mInterpolator.getStabilizerLevel())
    {
        mInterpolator.setStabilizerLevel(properties.stabilizerLevel);
    }

    QList<QPointF> p = mInterpolator.interpolateStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::VECTOR)
    {
        mCurrentWidth = 30;
        QColor pathColor = qPremultiply(mEditor->color()->frontColor().rgba());

        QPen pen(pathColor,
                 mCurrentWidth * mEditor->view()->scaling(),
                 Qt::NoPen,
                 Qt::RoundCap,
                 Qt::RoundJoin);

        if (p.size() == 4)
        {
            QPainterPath path(p[0]);
            path.cubicTo(p[1], p[2], p[3]);
            mScribbleArea->drawPath(path, pen, Qt::NoBrush, QPainter::CompositionMode_Source);
        }
    }
}

void BucketTool::drawCtrlStroke()
{
    // Mirror PenTool::drawStroke() pattern for Ctrl mode with fixed width
    QList<QPointF> p = mInterpolator.interpolateStroke();

    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;

    const qreal brushWidth = 2.0;
    const QColor strokeColor = mEditor->color()->frontColor();

    qreal brushStep = 0.5 * brushWidth;
    brushStep = qMax<qreal>(1.0, brushStep);

    QPointF a = mCtrlLastBrushPoint;
    QPointF b = getCurrentPoint();

    qreal distance = 4 * QLineF(b, a).length();
    if (distance <= 0.0) return;
    int steps = qMax(1, qRound(distance / brushStep));

    for (int i = 0; i < steps; ++i)
    {
        qreal t = qreal(i + 1) / qreal(steps);
        QPointF point = a + t * (b - a);
        mScribbleArea->drawPen(point, brushWidth, strokeColor, /*useAA*/ false);

        if (mCtrlStrokeHybridPoints.isEmpty() || QLineF(mCtrlStrokeHybridPoints.last(), point).length() >= 1.0)
        {
            mCtrlStrokeHybridPoints.append(point);
        }

        if (i == (steps - 1))
        {
            mCtrlLastBrushPoint = point;
        }
    }
}

void BucketTool::drawDragPreviewAt(const QPointF& currentPoint)
{
    // Map current canvas point to screen space
    const QPointF screenPt = mEditor->view()->mapCanvasToScreen(currentPoint);

    // Allocate buffer on first call (sized to the scribble area viewport)
    if (mDragOverlayBuffer.isNull()) {
        const QSize viewSize = mScribbleArea->size();
        mDragOverlayBuffer = QImage(viewSize, QImage::Format_ARGB32_Premultiplied);
        mDragOverlayBuffer.fill(Qt::transparent);

        // Draw the start dot once
        const QPointF startScreen = mEditor->view()->mapCanvasToScreen(mStartPoint);
        mDragStartScreenPoint = startScreen;
        mDragLastScreenPoint  = startScreen;
        mDragBufferHasStart   = true;

        const QColor accent = CosmicColor::accent();
        const qreal dotR   = 4.0;
        const qreal borderW = 1.5;

        QPainter p(&mDragOverlayBuffer);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        // white border dot
        p.setBrush(QColor(255, 255, 255, 220));
        p.drawEllipse(startScreen, dotR + borderW, dotR + borderW);
        // accent dot
        p.setBrush(accent);
        p.drawEllipse(startScreen, dotR, dotR);
    }

    // Skip if the point hasn't moved enough (0.5 px threshold)
    if (QLineF(mDragLastScreenPoint, screenPt).length() < 0.5)
        return;

    const QColor accent = CosmicColor::accent();
    const qreal strokeW = 2.0;
    const qreal borderW = 1.5;

    // Paint only the new segment into the buffer
    QPainter p(&mDragOverlayBuffer);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // White border segment
    QPen borderPen(QColor(255, 255, 255, 220), strokeW + borderW * 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(borderPen);
    p.drawLine(mDragLastScreenPoint, screenPt);

    // Accent segment on top
    QPen accentPen(accent, strokeW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(accentPen);
    p.drawLine(mDragLastScreenPoint, screenPt);

    mDragLastScreenPoint = screenPt;

    // Trigger repaint — full widget repaint is cheap since paint() just blits one QImage
    mScribbleArea->update();
}

void BucketTool::drawDragPreviewOverlay(QPainter& painter)
{
    if (mDragOverlayBuffer.isNull()) return;

    const QColor accent = CosmicColor::accent();
    const qreal dotR   = 4.0;
    const qreal borderW = 1.5;

    painter.save();
    painter.setWorldMatrixEnabled(false);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Blit the accumulated stroke buffer
    painter.drawImage(0, 0, mDragOverlayBuffer);

    // Draw the live end dot on top (no copy needed)
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.drawEllipse(mDragLastScreenPoint, dotR + borderW, dotR + borderW);
    painter.setBrush(accent);
    painter.drawEllipse(mDragLastScreenPoint, dotR, dotR);

    painter.restore();
}

void BucketTool::drawGapDebugOverlay(QPainter& painter)
{
    if (!mGapDebugOverlayActive) {
        return;
    }

    auto drawDiagnostic = [&](const QString& text) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setWorldMatrixEnabled(false);
        QPointF screenPos = mEditor->view()->mapCanvasToScreen(mGapDebugClickPoint);
        painter.setPen(QPen(QColor(255, 200, 0)));
        painter.setBrush(Qt::NoBrush);
        painter.drawText(screenPos + QPointF(12, -12), text);
        painter.restore();
    };

    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) {
        drawDiagnostic(tr("Gap debug works only on bitmap layers"));
        return;
    }
    if (properties.bucketGapFillMode <= 0) {
        drawDiagnostic(tr("Gap Fill is Off"));
        return;
    }

    auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* cellImg = bitmapLayer->getLastBitmapImageAtFrame(mEditor->currentFrame(), 0);
    if (!cellImg) {
        drawDiagnostic(tr("No bitmap image at this frame"));
        return;
    }

    const auto& strokes = cellImg->hybridStrokes();
    if (strokes.isEmpty()) {
        drawDiagnostic(tr("No hybrid strokes recorded (use Pen/Curve/Polyline or Ctrl+drag bucket)"));
        return;
    }

    // Map gap fill mode to radius (must match BitmapBucket::applyGapFillBridges)
    qreal gapRadius = 0.0;
    switch (properties.bucketGapFillMode) {
    case 1: gapRadius = 10.0; break;  // Low
    case 2: gapRadius = 25.0; break;  // Medium
    case 3: gapRadius = 40.0; break;  // High
    default: break;
    }
    if (gapRadius <= 0.0) {
        return;
    }
    const qreal gapRadiusSq = gapRadius * gapRadius;

    struct Endpoint {
        QPointF pos;
        int strokeIndex = -1;
        bool isStart = false;
        qreal angle = 0.0;
        bool paired = false;
    };

    QVector<Endpoint> endpoints;
    endpoints.reserve(strokes.size() * 2);
    for (int si = 0; si < strokes.size(); ++si) {
        const auto& s = strokes[si];
        if (s.points.size() < 2) {
            continue;
        }
        Endpoint eStart;
        eStart.pos = s.points.first();
        eStart.strokeIndex = si;
        eStart.isStart = true;
        endpoints.push_back(eStart);

        Endpoint eEnd;
        eEnd.pos = s.points.last();
        eEnd.strokeIndex = si;
        eEnd.isStart = false;
        endpoints.push_back(eEnd);
    }

    if (endpoints.size() < 2) {
        return;
    }

    // Compute polar angle around the debug click point
    const QPointF origin = mGapDebugClickPoint;
    for (Endpoint& e : endpoints) {
        const qreal dx = e.pos.x() - origin.x();
        const qreal dy = e.pos.y() - origin.y();
        e.angle = qAtan2(dy, dx);
    }

    std::sort(endpoints.begin(), endpoints.end(), [](const Endpoint& a, const Endpoint& b){
        return a.angle < b.angle;
    });

    // Determine bridge pairs using the same angular neighbor strategy
    const int n = endpoints.size();
    QVector<QPair<int,int>> pairs;
    pairs.reserve(n / 2);
    for (int i = 0; i < n; ++i) {
        Endpoint& e = endpoints[i];
        if (e.paired) {
            continue;
        }

        int bestIndex = -1;
        qreal bestD2 = gapRadiusSq;
        for (int offset = 1; offset < n; ++offset) {
            int j = i + offset;
            if (j >= n) break;
            Endpoint& f = endpoints[j];
            if (f.paired) {
                continue;
            }
            const qreal dx = f.pos.x() - e.pos.x();
            const qreal dy = f.pos.y() - e.pos.y();
            const qreal d2 = dx * dx + dy * dy;
            if (d2 <= bestD2) {
                bestD2 = d2;
                bestIndex = j;
            }
        }

        if (bestIndex < 0) {
            continue;
        }

        endpoints[i].paired = true;
        endpoints[bestIndex].paired = true;
        pairs.append(qMakePair(i, bestIndex));
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // Draw in screen space so radii/widths are consistent regardless of zoom
    painter.setWorldMatrixEnabled(false);

    ViewManager* view = mEditor->view();

    // Draw hybrid centerlines in blue
    QPen centerPen(QColor(0, 0, 255), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    centerPen.setCosmetic(true);
    painter.setPen(centerPen);
    for (const auto& s : strokes) {
        if (s.points.size() < 2) continue;
        QPainterPath path(view->mapCanvasToScreen(s.points.first()));
        for (int i = 1; i < s.points.size(); ++i) {
            path.lineTo(view->mapCanvasToScreen(s.points[i]));
        }
        painter.drawPath(path);
    }

    // Draw endpoints as thick red dots (screen-space radius)
    QBrush redBrush(Qt::red);
    painter.setBrush(redBrush);
    painter.setPen(Qt::NoPen);
    const qreal radius = 4.0;
    for (const Endpoint& e : endpoints) {
        QPointF sp = view->mapCanvasToScreen(e.pos);
        painter.drawEllipse(sp, radius, radius);
    }

    // Draw bridge pairs in pink
    QPen bridgePen(QColor(255, 0, 255), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    bridgePen.setCosmetic(true);
    painter.setPen(bridgePen);
    painter.setBrush(Qt::NoBrush);
    for (const auto& pr : pairs) {
        const Endpoint& a = endpoints[pr.first];
        const Endpoint& b = endpoints[pr.second];
        QPointF sa = view->mapCanvasToScreen(a.pos);
        QPointF sb = view->mapCanvasToScreen(b.pos);
        painter.drawLine(sa, sb);
    }

    painter.restore();
}

bool BucketTool::hitGizmoScreen(const QPointF& screenPos) const
{
    if (!mGizmoActive) return false;
    QPointF gizmoScreen = mEditor->view()->mapCanvasToScreen(mGizmoCanvasPos);
    qreal dist = QLineF(gizmoScreen, screenPos).length();
    return dist <= mGizmoRadiusScreen;
}

void BucketTool::drawGizmo(QPainter& painter)
{
    painter.save();
    painter.setWorldMatrixEnabled(false);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPointF centerScreen = mEditor->view()->mapCanvasToScreen(mGizmoCanvasPos);
    qreal r = mGizmoRadiusScreen;

    // Blue circle outline
    QPen ringPen(QColor(0, 122, 255), 2.0);
    painter.setPen(ringPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(centerScreen, r, r);

    // Cross icon (coded SVG-like): two perpendicular lines
    QPen crossPen(QColor(0, 122, 255), 2.0, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(crossPen);
    painter.drawLine(QPointF(centerScreen.x() - r * 0.6, centerScreen.y()),
                     QPointF(centerScreen.x() + r * 0.6, centerScreen.y()));
    painter.drawLine(QPointF(centerScreen.x(), centerScreen.y() - r * 0.6),
                     QPointF(centerScreen.x(), centerScreen.y() + r * 0.6));

    painter.restore();
}

void BucketTool::performRangeFillFromGizmo(int currentFrame)
{
    // Only for bitmap layer
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;
    if (mGizmoStartFrame < 0 || currentFrame == mGizmoStartFrame) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);

    int from = qMin(mGizmoStartFrame, currentFrame);
    int to = qMax(mGizmoStartFrame, currentFrame);

    // Prepare a BitmapBucket configured at the gizmo position; we will update editor frame per iteration
    LayerCamera* layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
    QRect cameraBounds = layerCam ? layerCam->getViewAtFrame(currentFrame).inverted().mapRect(layerCam->getViewRect()) : QRect();

    QSet<BitmapImage*> visitedCells;

    for (int f = from; f <= to; ++f)
    {
        // Scrub to frame f without disrupting selection/overlays
        mEditor->scrubTo(f);
        // Active keyframe context is refreshed by scrubTo(); proceed to handle empty frames
        // Ensure drawing on empty frames is handled according to preferences (may create keyframes)
        mScribbleArea->handleDrawingOnEmptyFrame();

        BitmapImage* targetImage = bitmapLayer->getLastBitmapImageAtFrame(f, 0);
        if (!targetImage) {
            continue;
        }

        if (visitedCells.contains(targetImage)) {
            continue;
        }
        visitedCells.insert(targetImage);

        // Recompute camera rect for this specific frame
        LayerCamera* layerCamLocal = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
        QRect camRect = layerCamLocal ? layerCamLocal->getViewAtFrame(f).inverted().mapRect(layerCamLocal->getViewRect()) : QRect();

        BitmapBucket frameBucket(mEditor,
                                 mEditor->color()->frontColor(),
                                 camRect,
                                 mGizmoCanvasPos,
                                 properties,
                                 mEditor->autoLineFillEnabled(),
                                 mEditor->autoLineColors(),
                                 mEditor->ignoreFillColor(),
                                 mEditor->ignoreFillEnabled());

        // Only capture undo state if this frame has an actual keyframe (not a hold frame)
        const UndoSaveState* saveState = nullptr;
        bool isActualKeyframe = layer->keyExists(f);
        if (isActualKeyframe && mEditor && mEditor->undoRedo()) {
            saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
        }

        // Track whether a fill actually occurred for this frame
        bool didFill = false;

        frameBucket.paint(mGizmoCanvasPos, [this, &didFill](BucketState progress, int layerIndex, int frameIndex){
            if (progress == BucketState::DidFillTarget) {
                didFill = true;
                mEditor->setModified(layerIndex, frameIndex);
            }
        });

        if (didFill) {
            // Commit the tiled buffer to the bitmap image for this frame
            mScribbleArea->paintBitmapBuffer();

            // Record undo ONLY for actual keyframes, not hold frames
            // This ensures one undo entry per keyframe, not per frame
            if (isActualKeyframe && mEditor && mEditor->undoRedo() && saveState) {
                mEditor->undoRedo()->record(saveState, tr("Fill Keyframe at Frame %1").arg(f));
            }
        }
    }

    // Return to the originally clicked current frame (the one that triggered the operation)
    mEditor->scrubTo(currentFrame);

}

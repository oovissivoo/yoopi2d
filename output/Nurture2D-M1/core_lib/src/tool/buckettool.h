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

#ifndef BUCKETTOOL_H
#define BUCKETTOOL_H

#include "stroketool.h"

#include "bitmapimage.h"
#include "bitmapbucket.h"
#include <QImage>

class Layer;
class VectorImage;

class BucketTool : public StrokeTool
{
    Q_OBJECT
public:
    explicit BucketTool(QObject* parent = nullptr);
    ToolType type() override;
    void loadSettings() override;
    void saveSettings() override;
    void resetToDefault() override;
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void paint(QPainter& painter, const QRect& blitRect) override;

    void setTolerance(const int tolerance) override;
    void setToleranceEnabled(const bool enabled) override;
    void setWidth(const qreal width) override;
    void setFillExpand(const int fillExpandValue) override;
    void setFillExpandEnabled(const bool enabled) override;
    void setFillReferenceMode(int referenceMode) override;
    void setFillMode(int mode) override;

    void paintBitmap();
    void paintVector(Layer* layer);
    void drawStroke();

    void applyChanges();

private:
    void drawCtrlStroke();

    BitmapBucket mBitmapBucket;
    VectorImage* vectorImage = nullptr;

    bool mFilledOnMove = false;

    // Drag preview state — raster overlay buffer (incremental, O(1) per segment)
    bool mDragging = false;
    QPointF mStartPoint;
    QImage mDragOverlayBuffer;          // screen-space ARGB32_Premultiplied buffer
    QPointF mDragLastScreenPoint;       // last committed screen point in buffer
    QPointF mDragStartScreenPoint;      // start dot position in screen space
    bool mDragBufferHasStart = false;   // whether start dot has been drawn

    void drawDragPreviewAt(const QPointF& currentPoint);
    void drawDragPreviewOverlay(QPainter& painter);

    // Gap-fill debug overlay: active while LMB is held for bucket fills
    bool mGapDebugOverlayActive = false;
    QPointF mGapDebugClickPoint;

    // Ctrl-drag temporary stroke mode using StrokeTool pipeline
    bool mCtrlStrokeActive = false;
    QPointF mCtrlMouseDownPoint;
    QPointF mCtrlLastBrushPoint;
    qreal mCtrlCurrentPressure = 0.5;
    bool mCtrlPrevDrawBehind = false;
    QVector<QPointF> mCtrlStrokeHybridPoints;

    // Multi-Keyframe Fill gizmo state
    bool mGizmoActive = false;            // true once first click places the gizmo
    bool mGizmoDragging = false;          // true while dragging gizmo
    int mGizmoStartFrame = -1;            // frame where gizmo was created
    QPointF mGizmoCanvasPos;              // gizmo position in canvas coordinates
    qreal mGizmoRadiusScreen = 12.0;      // hit radius in screen pixels
    bool mSuppressReleaseOnce = false;    // skip normal release handling after special press

    // Helpers for gizmo
    bool hitGizmoScreen(const QPointF& screenPos) const;
    void drawGizmo(QPainter& painter);
    void performRangeFillFromGizmo(int currentFrame);

    // Debug helper to visualize gap-fill endpoints and bridges on bitmap layers
    void drawGapDebugOverlay(QPainter& painter);

    // Undo macro state for grouped actions (e.g., Ctrl-stroke); multi-frame macro removed
    
    // Undo macro state for continuous (click-drag) fills
    bool mContinuousFillMacroActive = false;
    const UndoSaveState* mContinuousFillUndoState = nullptr;
};

#endif // BUCKETTOOL_H

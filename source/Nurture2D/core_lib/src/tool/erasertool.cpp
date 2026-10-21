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
#include "erasertool.h"

#include <QSettings>
#include "managers/settingsmanager.h"
#include <QPixmap>
#include <QPainter>

#include "editor.h"
#include "blitrect.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "undoredomanager.h"
#include "layervector.h"
#include "vectorimage.h"
#include "layerbitmap.h"
#include "pointerevent.h"
#include "selectionmanager.h"
#include "toolmanager.h"


EraserTool::EraserTool(QObject* parent) : StrokeTool(parent)
{
}

void EraserTool::endStroke()
{
    StrokeTool::endStroke();

    if (mEraseSamples.isEmpty()) {
        return;
    }

    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) {
        mEraseSamples.clear();
        mEraseRadii.clear();
        return;
    }
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* img = bitmapLayer->getLastBitmapImageAtFrame(mEditor->currentFrame());
    if (!img) {
        mEraseSamples.clear();
        mEraseRadii.clear();
        return;
    }

    img->eraseHybridStrokesWithBrush(mEraseSamples, mEraseRadii);
    mEraseSamples.clear();
    mEraseRadii.clear();
}

ToolType EraserTool::type()
{
    return ERASER;
}

void EraserTool::loadSettings()
{
    StrokeTool::loadSettings();

    mPropertyEnabled[WIDTH] = true;
    mPropertyEnabled[PRESSURE] = true;
    mPropertyEnabled[STABILIZATION] = false;
    mPropertyEnabled[ANTI_ALIASING] = false;

    SettingsManager* settings = SettingsManager::instance();

    properties.width = settings->getValue("eraserWidth", 24.0).toDouble();
    // Feather is disabled for the eraser; always use a hard edge
    properties.feather = 0.0;
    properties.useFeather = false;
    properties.pressure = settings->getValue("eraserPressure", true).toBool();
    properties.invisibility = DISABLED;
    properties.preserveAlpha = OFF;
    properties.stabilizerLevel = StabilizationLevel::NONE;  // Base Stabiliser always set to None
    // Normal eraser uses non-antialiased strokes; keep OFF for clarity
    properties.useAA = OFF;
    properties.eraserLassoMode = settings->getValue("eraserLassoMode", false).toBool();

}

void EraserTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();

    settings->deferredSetValue("eraserWidth", properties.width);
    settings->deferredSetValue("eraserPressure", properties.pressure);
    settings->deferredSetValue("eraserAA", properties.useAA);
    // Base Stabiliser removed - always NONE
    settings->deferredSetValue("eraserLassoMode", properties.eraserLassoMode);
    clearSettingsDirty();
}

void EraserTool::resetToDefault()
{
    setWidth(24.0);
    setPressure(true);
    // Normal eraser uses hard, non-antialiased erase
    setAA(OFF);
    setStabilizerLevel(StabilizationLevel::NONE);
}

void EraserTool::setWidth(const qreal width)
{
    if (!qFuzzyCompare(properties.width, width)) { properties.width = width; markSettingsDirty(); }
}

void EraserTool::setUseFeather(const bool usingFeather)
{
    if (properties.useFeather != usingFeather) { properties.useFeather = usingFeather; markSettingsDirty(); }
}

void EraserTool::setFeather(const qreal feather)
{
    if (!qFuzzyCompare(properties.feather, feather)) { properties.feather = feather; markSettingsDirty(); }
}

void EraserTool::setPressure(const bool pressure)
{
    if (properties.pressure != pressure) { properties.pressure = pressure; markSettingsDirty(); }
}

void EraserTool::setAA(const int AA)
{
    if (properties.useAA != AA) { properties.useAA = AA; markSettingsDirty(); }
}

void EraserTool::setStabilizerLevel(const int level)
{
    if (properties.stabilizerLevel != level) { properties.stabilizerLevel = level; markSettingsDirty(); }
}


QCursor EraserTool::cursor()
{
    // When in lasso erase mode, use the same cursor as lasso gap fill tool
    if (properties.eraserLassoMode)
    {
        if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
        {
            return QCursor(QPixmap(":/icons/themes/playful/general/cursor-crosshair.svg"), 8, 8);
        }
        return Qt::CrossCursor;
    }
    // Normal eraser mode uses blank cursor (brush circle is drawn separately)
    return Qt::BlankCursor;
}

void EraserTool::pointerPressEvent(PointerEvent *event)
{
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    startStroke(event->inputType());
    mEraseSamples.clear();
    mEraseRadii.clear();
    mLastBrushPoint = getCurrentPoint();
    mMouseDownPoint = getCurrentPoint();

    // Begin an undo macro to group all operations during the stroke
    // into a single undo entry.
    if (mEditor && mEditor->undoRedo()) {
        mEditor->undoRedo()->undoStack()->beginMacro(typeName());
        mUndoMacroActive = true;
    }

    // If lasso mode is enabled, begin lasso selection instead of painting
    if (properties.eraserLassoMode && (event->buttons() & Qt::LeftButton)) {
        auto sel = mEditor->select();
        sel->beginLassoAt(getCurrentPoint());
        mLassoErasing = true;
    }

    StrokeTool::pointerPressEvent(event);
}

void EraserTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->buttons() & Qt::LeftButton && event->inputType() == mCurrentInputType)
    {
        if (properties.eraserLassoMode) {
            if (mLassoErasing) {
                mEditor->select()->appendLassoPoint(getCurrentPoint());
            }
        } else {
            mCurrentPressure = mInterpolator.getPressure();
            updateStrokes();
            if (properties.stabilizerLevel != mInterpolator.getStabilizerLevel())
            {
                mInterpolator.setStabilizerLevel(properties.stabilizerLevel);
            }
        }
    }

    StrokeTool::pointerMoveEvent(event);
}

void EraserTool::pointerReleaseEvent(PointerEvent *event)
{
    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->inputType() != mCurrentInputType) {
        // Clean up macro if we started one
        if (mUndoMacroActive && mEditor && mEditor->undoRedo()) {
            mEditor->undoRedo()->undoStack()->endMacro();
            mUndoMacroActive = false;
        }
        return;
    }

    if (properties.eraserLassoMode) {
        if (mLassoErasing) {
            auto sel = mEditor->select();
            sel->closeLasso();
            // cut selection immediately
            sel->extractFloatingFromPath(true);
            // Clear selection visuals after delete
            sel->clearAll();
            mLassoErasing = false;
        }
        removeVectorPaint();
        endStroke();
    } else {
        qreal distance = QLineF(getCurrentPoint(), mMouseDownPoint).length();
        if (distance < 1)
        {
            paintAt(mMouseDownPoint);
        }
        else
        {
            drawStroke();
        }

        removeVectorPaint();
        endStroke();
    }

    // Record undo entry inside the macro to group all operations
    if (mEditor && mEditor->undoRedo() && mUndoSaveState) {
        mEditor->undoRedo()->record(mUndoSaveState, typeName());
        mUndoSaveState = nullptr; // Prevent double-recording in endStroke()
    }

    // End the undo macro if we started one
    if (mUndoMacroActive && mEditor && mEditor->undoRedo()) {
        mEditor->undoRedo()->undoStack()->endMacro();
        mUndoMacroActive = false;
    }

    StrokeTool::pointerReleaseEvent(event);
}

// draw a single paint dab at the given location
void EraserTool::paintAt(QPointF point)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        // Compute effective width without mutating properties so UI slider stays global
        qreal pressure = (properties.pressure) ? mCurrentPressure : 1.0;
        qreal baseWidth = properties.width;
        if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
            const qreal gw = mEditor->tools()->globalWidth();
            if (gw > 0.0) {
                baseWidth = qMax<qreal>(1.0, gw + 18.0);
            }
        }
        qreal opacity = (properties.pressure) ? (mCurrentPressure * 0.5) : 1.0;
        qreal brushWidth = baseWidth * pressure;
        mCurrentWidth = brushWidth;

        mScribbleArea->drawBrush(point,
                                 brushWidth,
                                 0.0,
                                 QColor(255, 255, 255, 255),
                                 QPainter::CompositionMode_SourceOver,
                                 opacity,
                                 false,
                                 false);
        mEraseSamples.append(point);
        mEraseRadii.append(0.5 * brushWidth);
        // Overlay interactions removed
    }
}

void EraserTool::drawStroke()
{
    StrokeTool::drawStroke();
    QList<QPointF> p = mInterpolator.interpolateStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::BITMAP)
    {
        qreal pressure = (properties.pressure) ? mCurrentPressure : 1.0;
        qreal opacity = (properties.pressure) ? (mCurrentPressure * 0.5) : 1.0;
        qreal baseWidth = properties.width;
        if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
            const qreal gw = mEditor->tools()->globalWidth();
            if (gw > 0.0) {
                baseWidth = qMax<qreal>(1.0, gw + 18.0);
            }
        }
        qreal brushWidth = baseWidth * pressure;
        mCurrentWidth = brushWidth;

        qreal brushStep = (0.5 * brushWidth);
        brushStep = qMax(1.0, brushStep);

        BlitRect rect;

        QPointF a = mLastBrushPoint;
        QPointF b = getCurrentPoint();

        qreal distance = 4 * QLineF(b, a).length();
        int steps = qRound(distance / brushStep);

        for (int i = 0; i < steps; i++)
        {
            QPointF point = mLastBrushPoint + (i + 1) * brushStep * (getCurrentPoint() - mLastBrushPoint) / distance;

            mScribbleArea->drawBrush(point,
                                     brushWidth,
                                     0.0,
                                     Qt::white,
                                     QPainter::CompositionMode_SourceOver,
                                     opacity,
                                     false,
                                     false);
            mEraseSamples.append(point);
            mEraseRadii.append(0.5 * brushWidth);
            if (i == (steps - 1))
            {
                mLastBrushPoint = getCurrentPoint();
            }
        }
        // Overlay interactions removed
    }
    else if (layer->type() == Layer::VECTOR)
    {
        qreal baseWidth = properties.width;
        if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
            const qreal gw = mEditor->tools()->globalWidth();
            if (gw > 0.0) {
                baseWidth = qMax<qreal>(1.0, gw + 18.0);
            }
        }
        mCurrentWidth = baseWidth;
        if (properties.pressure)
        {
            mCurrentWidth = (mCurrentWidth + (mInterpolator.getPressure() * mCurrentWidth)) * 0.5;
        }
        qreal brushWidth = mCurrentWidth;

        QPen pen(Qt::white, brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

        if (p.size() == 4)
        {
            QPainterPath path(p[0]);
            path.cubicTo(p[1],
                         p[2],
                         p[3]);
            mScribbleArea->drawPath(path, pen, Qt::NoBrush, QPainter::CompositionMode_Source);
        }
        // Overlay interactions removed
    }
}

void EraserTool::removeVectorPaint()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::VECTOR)
    {
        mScribbleArea->clearDrawingBuffer();
        VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame(), 0);
        if (vectorImage == nullptr) { return; } // Can happen if the first frame is deleted while drawing
        // Clear the area containing the last point
        //vectorImage->removeArea(lastPoint);
        // Clear the temporary pixel path
        vectorImage->deleteSelectedPoints();

        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    }
}

void EraserTool::updateStrokes()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR)
    {
        drawStroke();
    }

    if (layer->type() == Layer::VECTOR)
    {
        qreal radius = properties.width / 2;

        VectorImage* currKey = static_cast<VectorImage*>(layer->getLastKeyFrameAtPosition(mEditor->currentFrame()));
        QList<VertexRef> nearbyVertices = currKey->getVerticesCloseTo(getCurrentPoint(), radius);
        for (auto nearbyVertice : nearbyVertices)
        {
            currKey->setSelected(nearbyVertice, true);
        }
    }
}

// -- Lasso erase helpers (legacy no-ops retained for clarity) --
void EraserTool::beginLassoErase(const QPointF&) {}
void EraserTool::updateLassoErase(const QPointF&) {}
void EraserTool::finalizeLassoErase() {}

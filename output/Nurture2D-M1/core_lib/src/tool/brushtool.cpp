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

#include "brushtool.h"

#include <cmath>
#include <QSettings>
#include "managers/settingsmanager.h"
#include <QPixmap>
#include <QPainter>
#include <QColor>

#include "beziercurve.h"
#include "vectorimage.h"
#include "editor.h"
#include "colormanager.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "scribblearea.h"
#include "pointerevent.h"


BrushTool::BrushTool(QObject* parent) : StrokeTool(parent)
{
}

ToolType BrushTool::type()
{
    return BRUSH;
}

void BrushTool::loadSettings()
{
    StrokeTool::loadSettings();

    mPropertyEnabled[WIDTH] = true;
    mPropertyEnabled[FEATHER] = true;
    mPropertyEnabled[PRESSURE] = true;
    mPropertyEnabled[INVISIBILITY] = true;
    mPropertyEnabled[STABILIZATION] = true;
    mPropertyEnabled[DRAW_BEHIND] = true;

    SettingsManager* settings = SettingsManager::instance();

    properties.width = settings->getValue("brushWidth", 24.0).toDouble();
    properties.feather = settings->getValue("brushFeather", 48.0).toDouble();
    properties.pressure = settings->getValue("brushPressure", true).toBool();
    properties.invisibility = settings->getValue("brushInvisibility", false).toBool();
    properties.preserveAlpha = OFF;
    properties.stabilizerLevel = StabilizationLevel::NONE;  // Base Stabiliser always set to None
    properties.useAA = DISABLED;
    // Draw Behind is global; keep legacy read for migration but actual value comes from manager
    properties.drawBehind = settings->getValue("brushDrawBehind", false).toBool();

    if (properties.width <= 0) { setWidth(15); }
    if (std::isnan(properties.feather)) { setFeather(15); }

    mQuickSizingProperties.insert(Qt::ControlModifier, FEATHER);
}

void BrushTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();

    settings->deferredSetValue("brushWidth", properties.width);
    settings->deferredSetValue("brushFeather", properties.feather);
    settings->deferredSetValue("brushPressure", properties.pressure);
    settings->deferredSetValue("brushInvisibility", properties.invisibility);
    // Base Stabiliser removed - always NONE
    // Draw Behind is global; do not persist per-tool anymore (compat no-op)
    settings->deferredSetValue("brushDrawBehind", properties.drawBehind);
    clearSettingsDirty();
}

void BrushTool::resetToDefault()
{
    setWidth(24.0);
    setFeather(48.0);
    setStabilizerLevel(StabilizationLevel::NONE);  // Base Stabiliser always None
}

void BrushTool::setWidth(const qreal width)
{
    if (!qFuzzyCompare(properties.width, width)) { properties.width = width; markSettingsDirty(); }
}

void BrushTool::setFeather(const qreal feather)
{
    if (!qFuzzyCompare(properties.feather, feather)) { properties.feather = feather; markSettingsDirty(); }
}

void BrushTool::setInvisibility(const bool invisibility)
{
    if (properties.invisibility != invisibility) { properties.invisibility = invisibility; markSettingsDirty(); }
}

void BrushTool::setPressure(const bool pressure)
{
    if (properties.pressure != pressure) { properties.pressure = pressure; markSettingsDirty(); }
}

void BrushTool::setStabilizerLevel(const int level)
{
    if (properties.stabilizerLevel != level) { properties.stabilizerLevel = level; markSettingsDirty(); }
}

QCursor BrushTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":icons/general/cursor-brush.svg"), 4, 14);
    }
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void BrushTool::pointerPressEvent(PointerEvent *event)
{
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    mMouseDownPoint = getCurrentPoint();
    mLastBrushPoint = getCurrentPoint();

    startStroke(event->inputType());

    // Begin an undo macro to group all operations during the stroke
    // into a single undo entry.
    if (mEditor && mEditor->undoRedo()) {
        mEditor->undoRedo()->undoStack()->beginMacro(typeName());
        mUndoMacroActive = true;
    }

    StrokeTool::pointerPressEvent(event);
}

void BrushTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->buttons() & Qt::LeftButton && event->inputType() == mCurrentInputType)
    {
        mCurrentPressure = mInterpolator.getPressure();
        drawStroke();
        
        if (properties.stabilizerLevel != mInterpolator.getStabilizerLevel())
        {
            mInterpolator.setStabilizerLevel(properties.stabilizerLevel);
        }
    }

    StrokeTool::pointerMoveEvent(event);
}

void BrushTool::pointerReleaseEvent(PointerEvent *event)
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

    Layer* layer = mEditor->layers()->currentLayer();

    qreal distance = QLineF(getCurrentPoint(), mMouseDownPoint).length();
    if (distance < 1)
    {
        paintAt(mMouseDownPoint);
    }
    else
    {
        drawStroke();
    }

    if (layer->type() == Layer::VECTOR) {
        paintVectorStroke(layer);
    }

    endStroke();

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
void BrushTool::paintAt(QPointF point)
{
    //qDebug() << "Made a single dab at " << point;
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        qreal pressure = (properties.pressure) ? mCurrentPressure : 1.0;
        qreal opacity = (properties.pressure) ? (mCurrentPressure * 0.5) : 1.0;
        qreal brushWidth = properties.width * pressure;
        mCurrentWidth = brushWidth;
        mScribbleArea->drawBrush(point,
                                 brushWidth,
                                 properties.feather,
                                 mEditor->color()->frontColor(),
                                 QPainter::CompositionMode_SourceOver,
                                 opacity,
                                 true);
    }
}

void BrushTool::drawStroke()
{
    StrokeTool::drawStroke();
    QList<QPointF> p = mInterpolator.interpolateStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::BITMAP)
    {
        qreal pressure = (properties.pressure) ? mCurrentPressure : 1.0;
        qreal opacity = (properties.pressure) ? (mCurrentPressure * 0.5) : 1.0;
        qreal brushWidth = properties.width * pressure;
        mCurrentWidth = brushWidth;

        qreal brushStep = (0.5 * brushWidth);
        brushStep = qMax(1.0, brushStep);

        QPointF a = mLastBrushPoint;
        QPointF b = getCurrentPoint();

        qreal distance = 4 * QLineF(b, a).length();
        int steps = qRound(distance / brushStep);

        for (int i = 0; i < steps; i++)
        {
            QPointF point = mLastBrushPoint + (i + 1) * brushStep * (getCurrentPoint() - mLastBrushPoint) / distance;

            mScribbleArea->drawBrush(point,
                                     brushWidth,
                                     properties.feather,
                                     mEditor->color()->frontColor(),
                                     QPainter::CompositionMode_SourceOver,
                                     opacity,
                                     true);
            if (i == (steps - 1))
            {
                mLastBrushPoint = getCurrentPoint();
            }
        }

        // Line visualizer
        // for debugging
//        QPainterPath tempPath;

//        QPointF mappedMousePos = mEditor->view()->mapScreenToCanvas(strokeManager()->getMousePos());
//        tempPath.moveTo(getCurrentPoint());
//        tempPath.lineTo(mappedMousePos);

//        QPen pen( Qt::black,
//                   1,
//                   Qt::SolidLine,
//                   Qt::RoundCap,
//                   Qt::RoundJoin );
//        mScribbleArea->drawPolyline(tempPath, pen, true);

    }
    else if (layer->type() == Layer::VECTOR)
    {
        qreal pressure = (properties.pressure) ? mCurrentPressure : 1;
        qreal brushWidth = properties.width * pressure;

        QPen pen(mEditor->color()->frontColor(),
                 brushWidth,
                 Qt::SolidLine,
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

// This function uses the points from DrawStroke
// and turns them into vector lines.
void BrushTool::paintVectorStroke(Layer* layer)
{
    if (mStrokePoints.empty())
        return;

    if (layer->type() == Layer::VECTOR && mStrokePoints.size() > -1)
    {
        // Clear the temporary pixel path
        mScribbleArea->clearDrawingBuffer();
        qreal tol = mScribbleArea->getCurveSmoothing() / mEditor->view()->scaling();

        BezierCurve curve(mStrokePoints, mStrokePressures, tol);
        curve.setWidth(properties.width);
        curve.setFeather(properties.feather);
        curve.setFilled(false);
        curve.setInvisibility(properties.invisibility);
        curve.setVariableWidth(properties.pressure);
        curve.setColorNumber(mEditor->color()->frontColorNumber());

        VectorImage* vectorImage = static_cast<VectorImage*>(layer->getLastKeyFrameAtPosition(mEditor->currentFrame()));
        if (properties.drawBehind) {
            vectorImage->insertCurve(0, curve, mEditor->view()->scaling(), false);
        } else {
            vectorImage->addCurve(curve, mEditor->view()->scaling(), false);
        }

        if (vectorImage->isAnyCurveSelected() || mEditor->select()->somethingSelected())
        {
            mEditor->deselectAll();
        }

        vectorImage->setSelected(vectorImage->getLastCurveNumber(), true);

        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
        if (mScribbleArea) {
            mScribbleArea->hybridStack()->rerenderRasterFromVectors(mEditor->currentFrame());
        }
    }
}

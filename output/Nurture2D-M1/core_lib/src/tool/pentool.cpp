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
#include "pentool.h"

#include <QPixmap>
#include <QSettings>
#include <QCursor>
#include <QPainterPath>
#include <QPen>
#include <cmath>
#include "managers/settingsmanager.h"
#include "util/mathutils.h"

#include "vectorimage.h"
#include "layervector.h"
#include "colormanager.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "undoredomanager.h"
#include "selectionmanager.h"
#include "editor.h"
#include "scribblearea.h"
#include "blitrect.h"
#include "pointerevent.h"
#include "toolmanager.h"
#include "object.h"
#include "graphics/vector/colorref.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "graphics/bitmap/bitmapimage.h"


void PenTool::maybeFlushRealtime(quint64 eventTimestampMs)
{
    if (!mScribbleArea) return;

    // Only relevant for bitmap drawing where the tiled buffer uses deferred signals
    // during an active stroke.
    Layer* layer = (mEditor && mEditor->layers()) ? mEditor->layers()->currentLayer() : nullptr;
    if (!layer || layer->type() != Layer::BITMAP) return;

    // Throttle flushes to keep realtime preview smooth while avoiding per-event overhead.
    // Time-based: aim for up to ~120Hz.
    // Dab-based: ensure progress even if timestamps are missing or coarse.
    const quint64 now = eventTimestampMs;
    const quint64 intervalMs = 8;
    const int dabThreshold = 24;

    if ((now > 0 && (mLastRealtimeFlushTimestampMs == 0 || (now - mLastRealtimeFlushTimestampMs) >= intervalMs)) ||
        (mDabsSinceRealtimeFlush >= dabThreshold)) {
        mScribbleArea->flushDrawingStroke();
        mLastRealtimeFlushTimestampMs = now;
        mDabsSinceRealtimeFlush = 0;
    }

}


PenTool::PenTool(QObject* parent) : StrokeTool(parent)
{
}

void PenTool::loadSettings()
{
    StrokeTool::loadSettings();

    mPropertyEnabled[WIDTH] = true;
    mPropertyEnabled[PRESSURE] = true;
    mPropertyEnabled[VECTORMERGE] = true;
    mPropertyEnabled[ANTI_ALIASING] = true;
    mPropertyEnabled[STABILIZATION] = true;
    mPropertyEnabled[DRAW_BEHIND] = true;

    SettingsManager* settings = SettingsManager::instance();

    // Share the same width key as PencilTool so Pen and Pencil always start
    // at the same base size — they are unified in the UI and the user expects
    // one consistent stroke size regardless of which preset is active.
    properties.width = settings->getValue("pencilWidth", 8.0).toDouble();
    properties.pressure = settings->getValue("penPressure", true).toBool();
    properties.invisibility = OFF;
    properties.preserveAlpha = OFF;
    properties.useAA = settings->getValue("unifiedAA", 1).toInt();
    properties.stabilizerLevel = StabilizationLevel::NONE;  // Base Stabiliser always set to None
    // Post-stroke correction level (0..3)
    properties.postCorrectionLevel = settings->getValue("penPostCorrectionLevel", 0).toInt();
    // Krita-like modes (default to WEIGHTED). Migrate any legacy "Off" to WEIGHTED.
    properties.penSmoothingMode = static_cast<PenSmoothingMode>(settings->getValue("penSmoothingMode", (int)PenSmoothingMode::WEIGHTED).toInt());
    if (properties.penSmoothingMode == PenSmoothingMode::LEGACY ||
        static_cast<int>(properties.penSmoothingMode) == 3 /* was LACE, now removed */) {
        properties.penSmoothingMode = PenSmoothingMode::WEIGHTED;
    }
    properties.weightedDistance = settings->getValue("penWeightedDistance", 12.0).toDouble();
    properties.weightedStrokeEnding = settings->getValue("penWeightedStrokeEnding", 0.0).toDouble();
    properties.weightedSmoothPressure = settings->getValue("penWeightedSmoothPressure", false).toBool();
    properties.weightedScalableDistance = settings->getValue("penWeightedScalableDistance", true).toBool();
    properties.stab2Distance = settings->getValue("penStab2Distance", 16.0).toDouble();
    properties.stab2Delay = settings->getValue("penStab2Delay", 4.0).toDouble();
    properties.stab2FinishLine = settings->getValue("penStab2FinishLine", true).toBool();
    properties.stab2StabilizeSensors = settings->getValue("penStab2StabilizeSensors", false).toBool();
    properties.stab2ScalableDistance = settings->getValue("penStab2ScalableDistance", true).toBool();
    // Draw Behind is now global; keep reading for migration but ignore local persistence
    properties.drawBehind = settings->getValue("penDrawBehind", false).toBool();
    properties.vectorMergeEnabled = settings->getValue("penVectorMerge", false).toBool();

    // Load pen pressure curve (defaults to near-linear)
    // Load multi-point pressure curve if present
    QVariant v = settings->getValue("penPressureCurvePoints", QVariant());
    mPressureCurvePoints.clear();
    if (v.isValid() && v.canConvert<QVariantList>()) {
        QVariantList list = v.toList();
        // expect flat alternating x,y
        for (int i = 0; i + 1 < list.size(); i += 2) {
            qreal x = list[i].toDouble(); qreal y = list[i+1].toDouble();
            mPressureCurvePoints.append(QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0)));
        }
        if (mPressureCurvePoints.isEmpty()) {
            mPressureCurvePoints.append(QPointF(0.5, 0.5));
        }
    } else {
        // Backwards compatibility: only migrate if legacy keys exist
        QVariant v1x = settings->getValue("penPressureCurveP1X", QVariant());
        QVariant v1y = settings->getValue("penPressureCurveP1Y", QVariant());
        QVariant v2x = settings->getValue("penPressureCurveP2X", QVariant());
        QVariant v2y = settings->getValue("penPressureCurveP2Y", QVariant());
        if (v1x.isValid() && v1y.isValid() && v2x.isValid() && v2y.isValid()) {
            const qreal p1x = v1x.toDouble();
            const qreal p1y = v1y.toDouble();
            const qreal p2x = v2x.toDouble();
            const qreal p2y = v2y.toDouble();
            // Represent legacy Bezier as implicit endpoints + legacy control points
            mPressureCurvePoints.append(QPointF(0.0, 0.0));
            mPressureCurvePoints.append(QPointF(qBound(0.0, p1x, 1.0), qBound(0.0, p1y, 1.0)));
            mPressureCurvePoints.append(QPointF(qBound(0.0, p2x, 1.0), qBound(0.0, p2y, 1.0)));
            mPressureCurvePoints.append(QPointF(1.0, 1.0));
        } else {
            // Fresh install: single midpoint, evaluator has implicit endpoints
            mPressureCurvePoints.append(QPointF(0.5, 0.5));
        }
    }
    mPressureCurveEnabled = settings->getValue("penPressureCurveEnabled", true).toBool();

    // Initialize pressure curve LUT with loaded control points
    mPressureCurveLUT.setCurvePoints(mPressureCurvePoints);

    // Load pressure pipeline settings
    properties.pressureSmoothingAmount = settings->getValue("penPressureSmoothingAmount", 0.3).toDouble();
    properties.pressureTaperingStart = settings->getValue("penPressureTaperingStart", 0.0).toDouble();  // CHANGED from 0.5 to 0.0
    properties.pressureTaperingEnd = settings->getValue("penPressureTaperingEnd", 0.5).toDouble();
}

void PenTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();

    // Write to the shared "pencilWidth" key so both tools stay in sync across sessions.
    settings->deferredSetValue("pencilWidth", properties.width);
    settings->deferredSetValue("penPressure", properties.pressure);
    settings->deferredSetValue("unifiedAA", properties.useAA);
    // Base Stabiliser removed - always NONE
    settings->deferredSetValue("penPostCorrectionLevel", properties.postCorrectionLevel);
    settings->deferredSetValue("penSmoothingMode", (int)properties.penSmoothingMode);
    settings->deferredSetValue("penWeightedDistance", properties.weightedDistance);
    settings->deferredSetValue("penWeightedStrokeEnding", properties.weightedStrokeEnding);
    settings->deferredSetValue("penWeightedSmoothPressure", properties.weightedSmoothPressure);
    settings->deferredSetValue("penWeightedScalableDistance", properties.weightedScalableDistance);
    // removed: weighted stroke ending / smooth pressure / scalable
    settings->deferredSetValue("penStab2Distance", properties.stab2Distance);
    settings->deferredSetValue("penStab2Delay", properties.stab2Delay);
    settings->deferredSetValue("penStab2FinishLine", properties.stab2FinishLine);
    settings->deferredSetValue("penStab2StabilizeSensors", properties.stab2StabilizeSensors);
    settings->deferredSetValue("penStab2ScalableDistance", properties.stab2ScalableDistance);
    // Draw Behind is global; do not persist per-tool anymore (retain no-op for compatibility)
    settings->deferredSetValue("penDrawBehind", properties.drawBehind);
    settings->deferredSetValue("penVectorMerge", properties.vectorMergeEnabled);

    // Save pen pressure curve (serialize as flat x,y list)
    QVariantList flat;
    for (const QPointF& p : mPressureCurvePoints) {
        flat.append(p.x()); flat.append(p.y());
    }
    settings->deferredSetValue("penPressureCurvePoints", flat);
    settings->deferredSetValue("penPressureCurveEnabled", mPressureCurveEnabled);

    // Save pressure pipeline settings
    settings->deferredSetValue("penPressureSmoothingAmount", properties.pressureSmoothingAmount);
    settings->deferredSetValue("penPressureTaperingStart", properties.pressureTaperingStart);
    settings->deferredSetValue("penPressureTaperingEnd", properties.pressureTaperingEnd);

    clearSettingsDirty();
}

void PenTool::resetToDefault()
{
    setWidth(12.0);
    setUseFeather(false);
    setPressure(true);
    setStabilizerLevel(StabilizationLevel::STRONG);
    setAA(1);
    properties.postCorrectionLevel = 0;
    properties.penSmoothingMode = PenSmoothingMode::WEIGHTED;
    properties.weightedDistance = 12.0;
    properties.weightedStrokeEnding = 0.0;
    properties.weightedSmoothPressure = false;
    properties.weightedScalableDistance = true;
    properties.stab2Distance = 16.0;
    properties.stab2Delay = 4.0;
    properties.stab2FinishLine = true;
    properties.stab2StabilizeSensors = false;
    properties.stab2ScalableDistance = true;
    mPressureCurvePoints.clear();
    mPressureCurvePoints.append(QPointF(0.5, 0.5));
    mPressureCurveLUT.setCurvePoints(mPressureCurvePoints);
    properties.pressureSmoothingAmount = 0.3;
    properties.pressureTaperingStart = 0.0;  // CHANGED from 0.5 to 0.0
    properties.pressureTaperingEnd = 0.5;
}

void PenTool::setWidth(const qreal width)
{
    if (!qFuzzyCompare(properties.width, width)) { properties.width = width; markSettingsDirty(); }
}

void PenTool::setPressure(const bool pressure)
{
    if (properties.pressure != pressure) { properties.pressure = pressure; markSettingsDirty(); }
}

void PenTool::setAA(const int AA)
{
    if (properties.useAA != AA) { properties.useAA = AA; markSettingsDirty(); }

}

void PenTool::setStabilizerLevel(const int level)
{
    if (properties.stabilizerLevel != level) { properties.stabilizerLevel = level; markSettingsDirty(); }
}
void PenTool::setPressureCurve(const QVector<QPointF>& points)
{
    mPressureCurvePoints.clear();
    for (const QPointF& p : points) {
        mPressureCurvePoints.append(QPointF(qBound(0.0, p.x(), 1.0), qBound(0.0, p.y(), 1.0)));
    }
    if (mPressureCurvePoints.isEmpty()) mPressureCurvePoints.append(QPointF(0.5, 0.5));
    MathUtils::sortControlPointsByX(mPressureCurvePoints);
    mPressureCurveLUT.setCurvePoints(mPressureCurvePoints);
}

QVector<QPointF> PenTool::getPressureCurve() const
{
    return mPressureCurvePoints;
}

QCursor PenTool::cursor()
{
    return Qt::BlankCursor;
}

void PenTool::pointerPressEvent(PointerEvent *event)
{
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    mMouseDownPoint = getCurrentPoint();
    mLastBrushPoint = getCurrentPoint();
    mLastDabPoint = getCurrentPoint();
    
    // Reset stroke state for optimized rendering
    mAccumulatedDistance = 0.0;
    mStrokePoints.clear();
    mStrokePressures.clear();
    mStrokePoints.append(getCurrentPoint());
    mStrokePressures.append(0.5);
    mVisitedDabCells.clear();

    // Reset realtime flush throttle state
    mLastRealtimeFlushTimestampMs = event ? event->timestamp() : 0;
    mDabsSinceRealtimeFlush = 0;
    
    // Reset dab recording for post-correction retargeting
    mRecordedDabWidths.clear();
    mRecordedDabCumDist.clear();
    mRecordedCum = 0.0;

    // Reset final stroke geometry used for bitmap hybrid recording
    mFinalStrokePoints.clear();
    mFinalStrokeBaseWidth = 0.0;

    // Initialize pressure pipeline (stroke-level setup only).
    {
        // Configure pipeline with current settings
        mPressurePipeline.setSmoothingAmount(properties.pressureSmoothingAmount);
        mPressurePipeline.setTaperingStart(properties.pressureTaperingStart);
        mPressurePipeline.setTaperingEnd(properties.pressureTaperingEnd);

        // Get initial raw pressure
        qreal rawPressure = event->isTabletEvent() ? event->pressure() : mInterpolator.getPressure();
        if (rawPressure <= 0.0) rawPressure = 0.5; // Fallback

        // Begin pipeline but defer sampling until we actually emit a dab
        mPressurePipeline.beginStroke(rawPressure);
        mLastRawPressure = rawPressure;
        mCurrentPressure = rawPressure; // initial estimate; refined per dab via pipeline + curve

        // Initialize position tracking for pipeline
        mLastPipelinePoint = getCurrentPoint();
        mPipelineDistance = 0.0;

        // Initialize adaptive dab spacing state
        // Seed initial spacing multiplier from current interpolator velocity
        {
            qreal initVelocity = mInterpolator.getVelocity(); // screen px/s
            qreal initVelocityCanvas = initVelocity;
            if (mEditor && mEditor->view()) {
                const qreal s = mEditor->view()->scaling();
                if (s > 0.0) initVelocityCanvas = initVelocity / s;
            }

            const qreal slowThreshold = 50.0;   // px/s in canvas space
            const qreal fastThreshold = 500.0;  // px/s in canvas space
            qreal initSpacingMult;
            if (initVelocityCanvas <= slowThreshold) {
                initSpacingMult = 1.0;
            } else if (initVelocityCanvas >= fastThreshold) {
                initSpacingMult = 0.25;
            } else {
                qreal t = (initVelocityCanvas - slowThreshold) / (fastThreshold - slowThreshold);
                initSpacingMult = 1.0 - 0.75 * t;
                initSpacingMult = qBound<qreal>(0.25, initSpacingMult, 1.0);
            }
            mLastSpacingMult = initSpacingMult;
        }
    }

    startStroke(event->inputType());

    // Begin an undo macro to group all operations during the stroke
    // into a single undo entry.
    if (mEditor && mEditor->undoRedo()) {
        mEditor->undoRedo()->undoStack()->beginMacro(typeName());
        mUndoMacroActive = true;
    }
    // Ensure post-correction has a valid first pressure sample
    if (mPressureCurveEnabled && !mStrokePressures.isEmpty()) {
        if (mStrokePressures.back() <= 0.0) {
            const qreal seed = (mCurrentPressure > 0.0 ? mCurrentPressure : 0.5);
            mStrokePressures.back() = qBound<qreal>(0.0, seed, 1.0);
        }
    }

    // Forward Krita-like modes directly; Pull vs Delayed is now fully
    // independent from post-stroke correction, which is handled separately.
    mInterpolator.setPenSmoothingMode(properties.penSmoothingMode);
    mInterpolator.setWeightedDistance(properties.weightedDistance);
    mInterpolator.setWeightedStrokeEnding(properties.weightedStrokeEnding);
    mInterpolator.setWeightedSmoothPressure(properties.weightedSmoothPressure);
    mInterpolator.setWeightedScalableDistance(properties.weightedScalableDistance);
    mInterpolator.setStab2Distance(properties.stab2Distance);
    mInterpolator.setStab2Delay(properties.stab2Delay);
    mInterpolator.setStab2FinishLine(properties.stab2FinishLine);
    mInterpolator.setStab2StabilizeSensors(properties.stab2StabilizeSensors);
    mInterpolator.setStab2ScalableDistance(properties.stab2ScalableDistance);

    // Initialize Pull cursor tracking state
    if (properties.penSmoothingMode == PenSmoothingMode::PULL) {
        mStabilizerCursorActive = true;
        mStabilizerClampedCursorPos = getCurrentPoint();
        mStabilizerPrevDistance = 0.0;
    }

    // Ensure the stroke starts with an actual stamped dab on bitmap layers.
    // This prevents thin/low-pressure strokes from missing initial pixels when the first move
    // segment is shorter than dabSpacing.
    if (mEditor && mEditor->layers()) {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer && layer->type() == Layer::BITMAP) {
            paintAt(getCurrentPoint());
            mLastDabPoint = getCurrentPoint();
            mAccumulatedDistance = 0.0;

            // Ensure the first dab becomes visible immediately.
            if (event) {
                maybeFlushRealtime(event->timestamp());
            }
        }
    }

    StrokeTool::pointerPressEvent(event);
}

void PenTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    bool cursorUpdatedEarly = false;
    if (event && (event->buttons() & Qt::LeftButton) && mInterpolator.isActive() && !msIsAdjusting) {
        const bool smoothOff =
            (properties.penSmoothingMode == PenSmoothingMode::PULL)
                ? (properties.stab2Delay <= 0.0)
                : (properties.weightedDistance <= 0.0);
        if (!smoothOff) {
            // no early cursor update
        } else {
            Layer* layer = (mEditor && mEditor->layers()) ? mEditor->layers()->currentLayer() : nullptr;
            if (layer && layer->type() == Layer::BITMAP) {
                StrokeTool::pointerMoveEvent(event);
                cursorUpdatedEarly = true;
            }
        }
    }

    if (event->buttons() & Qt::LeftButton && event->inputType() == mCurrentInputType)
    {
        // Stage A: cache latest raw pressure and update stroke distance only.
        qreal rawPressure = event->isTabletEvent() ? event->pressure() : mInterpolator.getPressure();
        if (rawPressure <= 0.0) rawPressure = (mCurrentPressure > 0.0 ? mCurrentPressure : 0.5);
        mLastRawPressure = rawPressure;

        QPointF currentPoint = getCurrentPoint();
        qreal segmentDist = QLineF(mLastPipelinePoint, currentPoint).length();
        mPipelineDistance += segmentDist;
        mLastPipelinePoint = currentPoint;

        drawStroke();

        // Throttled flush keeps delayed/weighted stabilizer preview realtime
        // without paying the cost of flushing on every move.
        maybeFlushRealtime(event ? event->timestamp() : 0);
        
        // Note: Progressive commit is handled automatically by ScribbleArea
        // between strokes. No need to call updateProgressiveCommitDistance here
        // as it early-exits during active drawing for performance.
        
        if (properties.stabilizerLevel != mInterpolator.getStabilizerLevel())
        {
            mInterpolator.setStabilizerLevel(properties.stabilizerLevel);
        }
        // Sync Krita-like modes live
        mInterpolator.setPenSmoothingMode(properties.penSmoothingMode);
        mInterpolator.setWeightedDistance(properties.weightedDistance);
        mInterpolator.setWeightedStrokeEnding(properties.weightedStrokeEnding);
        mInterpolator.setWeightedSmoothPressure(properties.weightedSmoothPressure);
        mInterpolator.setWeightedScalableDistance(properties.weightedScalableDistance);
        mInterpolator.setStab2Distance(properties.stab2Distance);
        mInterpolator.setStab2Delay(properties.stab2Delay);
        mInterpolator.setStab2FinishLine(properties.stab2FinishLine);
        mInterpolator.setStab2StabilizeSensors(properties.stab2StabilizeSensors);
        mInterpolator.setStab2ScalableDistance(properties.stab2ScalableDistance);
    }

    // Pull cursor clamping and OS cursor restriction
    // New behavior: while drawing in Pull mode on bitmap layers, restrict the OS cursor
    // to never be more than 1px outside the deadzone radius from the pull anchor.
    // Important: only do this while a stroke is actively in progress; otherwise we can
    // warp the cursor during navigation/focus changes and it will look like the cursor
    // is "jumping".
    if (mStabilizerCursorActive &&
        properties.penSmoothingMode == PenSmoothingMode::PULL &&
        mInterpolator.isActive() &&
        (event->buttons() & Qt::LeftButton)) {
        // Get positions in viewport pixels
        QPointF rawPixel = mInterpolator.getLastRawPixel();
        QPointF anchorPixel = mInterpolator.getStab2AnchorPixel();
        qreal deadzoneRadius = properties.stab2Delay;

        // Calculate vector from anchor to raw cursor
        QPointF toRaw = rawPixel - anchorPixel;
        qreal distToRaw = QLineF(QPointF(), toRaw).length();

        // Clamped visual cursor (used by rendering): clamp to deadzone boundary when outside
        QPointF clampedPixel;
        if (distToRaw > deadzoneRadius && distToRaw > 0.0) {
            QPointF normalized = toRaw / distToRaw;
            clampedPixel = anchorPixel + normalized * deadzoneRadius;
        } else {
            clampedPixel = rawPixel; // within deadzone
        }

        // Convert to canvas coordinates for any consumers of the clamped position
        mStabilizerClampedCursorPos = mEditor->view()->mapScreenToCanvas(clampedPixel);

        // OS cursor restriction: if the raw OS cursor exceeds deadzone+1px, warp it back
        // Only apply this when drawing on bitmap layers as requested
        Layer* curLayer = mEditor->layers()->currentLayer();
        if (curLayer && curLayer->type() == Layer::BITMAP) {
            const qreal allowed = qMax<qreal>(0.0, deadzoneRadius) + 1.0; // deadzone + 1px margin
            if (distToRaw > allowed && distToRaw > 0.0) {
                QPointF normalized = toRaw / distToRaw;
                QPointF limitedPixel = anchorPixel + normalized * allowed;
                QPoint globalPos = mScribbleArea->mapToGlobal(limitedPixel.toPoint());
                QCursor::setPos(globalPos);
            }
        }
        // Note: removed decreasing-distance snap-to-edge behavior; OS cursor is strictly limited by allowed radius.
    }

    if (!cursorUpdatedEarly) {
        StrokeTool::pointerMoveEvent(event);
    }
}

void PenTool::pointerReleaseEvent(PointerEvent *event)
{
    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        // Quick sizing mode can consume the release event; ensure Pull-mode state is cleared
        // so we don't keep warping the OS cursor on subsequent moves.
        mStabilizerCursorActive = false;
        return;
    }

    if (event->inputType() != mCurrentInputType) {
        // Clean up macro if we started one
        mVisitedDabCells.clear();
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
        // Single-tap: evaluate pressure once for this dab.
        if (properties.pressure) {
            qreal rawPressure = event->isTabletEvent() ? event->pressure() : mLastRawPressure;
            if (rawPressure <= 0.0) rawPressure = (mCurrentPressure > 0.0 ? mCurrentPressure : 0.5);
            QPointF delta(0.0, 0.0);
            qreal pipelinePressure = mPressurePipeline.sample(rawPressure, 0.0, mPipelineDistance, delta);
            mCurrentPressure = mPressureCurveEnabled ? mPressureCurveLUT.evaluate(pipelinePressure) : pipelinePressure;
        }
        paintAt(mMouseDownPoint);
    }
    else
    {
        drawStroke();
    }

    // Flush accumulated dabs before post-correction
    mScribbleArea->flushDrawingStroke();

    // Reset realtime flush throttle state
    mLastRealtimeFlushTimestampMs = 0;
    mDabsSinceRealtimeFlush = 0;

    if (layer->type() == Layer::VECTOR) {
        paintVectorStroke(layer);
    }
    // Apply post-stroke correction on bitmap strokes when enabled. Both
    // Delayed (Weighted) and String (Pull) modes respect the same
    // postCorrectionLevel so behavior comes from a single source of truth.
    if (layer->type() == Layer::BITMAP &&
        properties.postCorrectionLevel > 0) {
        applyPostStrokeCorrectionBitmap();
    }

    // Record final bitmap stroke geometry for hybrid vector storage
    if (layer->type() == Layer::BITMAP && mEditor && mEditor->layers()) {
        if (mFinalStrokePoints.isEmpty()) {
            mFinalStrokePoints = QVector<QPointF>::fromList(mStrokePoints);
        }
        if (!mFinalStrokePoints.isEmpty()) {
            qreal baseWidth = properties.width;
            if (!mRecordedDabWidths.isEmpty()) {
                qreal sum = 0.0;
                for (qreal w : mRecordedDabWidths) {
                    sum += w;
                }
                baseWidth = sum / static_cast<qreal>(mRecordedDabWidths.size());
            }
            mFinalStrokeBaseWidth = baseWidth;

            auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
            KeyFrame* _kf = bitmapLayer->getLastKeyFrameAtPosition(mEditor->currentFrame());
            BitmapKeyFrame* _bkf = _kf ? static_cast<BitmapKeyFrame*>(_kf) : nullptr;
            BitmapImage* cellImg = _bkf ? bitmapLayer->cellBank()->getActiveInnerLayerImage(_bkf->cellId()) : nullptr;
            if (cellImg) {
                cellImg->addHybridStroke(mFinalStrokePoints, mFinalStrokeBaseWidth);
            }
        }
    }
    
    // Finalize pressure pipeline
    mPressurePipeline.endStroke();
    
    endStroke();
    mVisitedDabCells.clear();

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

    // NOTE: Do not warp/snap the OS cursor on pen lift for any smoothing mode.
    // Warping here causes a visible "jump" at stroke end (especially in Pull/Delayed modes).
    // We still clean up Pull-mode state below.

    // Pull mode cleanup
    if (mStabilizerCursorActive && properties.penSmoothingMode == PenSmoothingMode::PULL) {
        mStabilizerCursorActive = false;
        mStabilizerPrevDistance = 0.0;
        mStabilizerClampedCursorPos = QPointF(0, 0);
    }

    StrokeTool::pointerReleaseEvent(event);
}

// Simple post-stroke correction: resample the recorded stroke points into a smoother path
// using a fixed window Savitzky-Golay-like averaging based on the selected level.
void PenTool::applyPostStrokeCorrectionBitmap()
{
    // Guard: need at least a few points
    const int n = mStrokePoints.size();
    if (n < 2) return;

    const int level = qBound(0, properties.postCorrectionLevel, 3);
    // Increased intensity: larger windows for Low/Med/High
    const int window = (level == 1) ? 5 : (level == 2) ? 9 : 13; // must be odd
    const int half = window / 2;

    QList<QPointF> smoothed; smoothed.reserve(n);
    QList<qreal>    smoothPress; smoothPress.reserve(n);
    for (int i = 0; i < n; ++i) {
        QPointF acc(0,0); int count = 0; qreal pacc = 0.0;
        for (int k = -half; k <= half; ++k) {
            const int j = i + k; if (j < 0 || j >= n) continue;
            acc += mStrokePoints[j];
            pacc += (j < mStrokePressures.size() ? mStrokePressures[j] : mStrokePressures.last());
            ++count;
        }
        smoothed.append(count > 0 ? QPointF(acc.x()/count, acc.y()/count) : mStrokePoints[i]);
        smoothPress.append(count > 0 ? pacc / count : (i < mStrokePressures.size() ? mStrokePressures[i] : 1.0));
    }

    // For highest level, apply a second pass for extra smoothing intensity
    if (level == 3) {
        QList<QPointF> smoothed2; smoothed2.reserve(n);
        QList<qreal>    smoothPress2; smoothPress2.reserve(n);
        for (int i = 0; i < n; ++i) {
            QPointF acc(0,0); int count = 0; qreal pacc = 0.0;
            for (int k = -half; k <= half; ++k) {
                const int j = i + k; if (j < 0 || j >= n) continue;
                acc += smoothed[j];
                pacc += (j < smoothPress.size() ? smoothPress[j] : smoothPress.last());
                ++count;
            }
            smoothed2.append(count > 0 ? QPointF(acc.x()/count, acc.y()/count) : smoothed[i]);
            smoothPress2.append(count > 0 ? pacc / count : (i < smoothPress.size() ? smoothPress[i] : 1.0));
        }
        smoothed.swap(smoothed2);
        smoothPress.swap(smoothPress2);
    }

    // Path-retargeting repaint: keep original recorded dab widths and cadence; align to smoothed path
    // If we have no recorded dabs (e.g., extremely short stroke), keep the live buffer as-is;
    // endStroke() will commit it. Do NOT clear the buffer in this case.
    if (mRecordedDabWidths.isEmpty()) {
        return;
    }
    // We are going to restamp: clear the live buffer now
    mScribbleArea->clearDrawingBuffer();

    // Store final smoothed path for bitmap hybrid stroke recording
    mFinalStrokePoints = QVector<QPointF>::fromList(smoothed);

    auto buildCumDist = [](const QList<QPointF>& pts){
        QVector<qreal> cum; cum.reserve(pts.size());
        qreal acc = 0.0; cum.push_back(0.0);
        for (int i = 1; i < pts.size(); ++i) { acc += QLineF(pts[i-1], pts[i]).length(); cum.push_back(acc); }
        return cum;
    };
    auto pointAtDistance = [](const QList<QPointF>& pts, const QVector<qreal>& cum, qreal d){
        if (pts.isEmpty()) return QPointF();
        if (d <= 0.0) return pts.first();
        if (d >= cum.last()) return pts.last();
        int i = 1; while (i < cum.size() && cum[i] < d) ++i; // first index with cum[i] >= d
        const qreal segStart = cum[i-1]; const qreal segLen = qMax<qreal>(1e-6, cum[i]-cum[i-1]);
        const qreal t = (d - segStart) / segLen; return pts[i-1] + t * (pts[i]-pts[i-1]);
    };
    // Removed unused scalarAtDistance lambda (was causing unused-variable warning)

    const QVector<qreal> origCum = buildCumDist(mStrokePoints);
    const QVector<qreal> smoothCum = buildCumDist(smoothed);
    const qreal origTotal = origCum.isEmpty() ? 0.0 : origCum.last();
    const qreal smoothTotal = smoothCum.isEmpty() ? 0.0 : smoothCum.last();
    if (origTotal <= 0.0 || smoothTotal <= 0.0) { return; }

    // Re-stamp using recorded dab widths at proportional arc lengths
    const int dabCount = mRecordedDabWidths.size();
    const qreal lastCum = mRecordedDabCumDist.isEmpty() ? 0.0 : mRecordedDabCumDist.back();
    // Adaptive stamping based on local drawing "speed" (approximated by local arc-length)
    // Strategy: compute smoothed target positions for recorded dabs, estimate local
    // movement (segLen) between neighboring recorded positions and map that to
    // a stamping multiplier. Faster movement (larger segLen) -> more samples;
    // slower movement -> fewer samples. This preserves recorded widths/pressures
    // while adapting sampling density to drawing speed.
    if (dabCount <= 0) {
        return;
    }

    // Precompute target positions along the smoothed path for each recorded dab
    QVector<QPointF> targetPos; targetPos.reserve(dabCount);
    QVector<qreal> targetWidth; targetWidth.reserve(dabCount);
    QVector<qreal> targetArc; targetArc.reserve(dabCount);
    const qreal denom = (dabCount > 1) ? qreal(dabCount - 1) : 1.0;
    for (int i = 0; i < dabCount; ++i) {
        const qreal d = (lastCum <= 0.0) ? (origTotal * (qreal(i) / denom)) : qBound<qreal>(0.0, (mRecordedDabCumDist[i] / lastCum) * origTotal, origTotal);
        const qreal sSmooth = (d / origTotal) * smoothTotal;
        targetPos.push_back(pointAtDistance(smoothed, smoothCum, sSmooth));
        targetWidth.push_back(mRecordedDabWidths[i]);
        targetArc.push_back(sSmooth);
    }

    // Parameters for mapping arc-length to stamping multiplier
    const qreal speedRef = 4.0;   // px: reference segment length where multiplier ~= 1
    const qreal speedScale = 40.0; // px: scale for multiplier change
    const qreal minMult = 0.5;    // minimum number of stamps per recorded dab
    const qreal maxMult = 3.0;    // maximum number of stamps per recorded dab

    // Redraw the corrected stroke using the same continuous-segment rendering as the
    // live bitmap pen (drawPathCanvas). This avoids the dotted look caused by sparse
    // dab stamping.
    if (dabCount == 1) {
        QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                                   ? mEditor->tools()->strokeColorOverride()
                                   : mEditor->color()->frontColor();
        strokeColor.setAlpha(255);
        const qreal width = targetWidth[0];
        mScribbleArea->drawPen(targetPos[0], width, strokeColor, false);
        return;
    }

    QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                               ? mEditor->tools()->strokeColorOverride()
                               : mEditor->color()->frontColor();
    strokeColor.setAlpha(255);

    for (int i = 0; i < dabCount - 1; ++i) {
        const QPointF a = targetPos[i];
        const QPointF b = targetPos[i + 1];
        const qreal segLen = QLineF(a, b).length();

        // Map segLen to additional subdivision so sharp/fast segments remain gapless.
        qreal mult = 1.0 + (segLen - speedRef) / speedScale;
        mult = qBound<qreal>(minMult, mult, maxMult);
        const int steps = qMax(1, qRound(mult));

        QPointF prev = a;
        qreal prevW = targetWidth[i];

        for (int s = 1; s <= steps; ++s) {
            const qreal t = qreal(s) / qreal(steps);
            const QPointF cur = a + t * (b - a);
            const qreal curW = prevW + t * (targetWidth[i + 1] - prevW);

            const qreal w = qMax<qreal>(1.0, 0.5 * (prevW + curW));
            QPainterPath path(prev);
            path.lineTo(cur);

            QPen pen(strokeColor);
            pen.setWidthF(w);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);

            // Keep bitmap pen strictly un-aliased.
            mScribbleArea->drawPathCanvas(path, pen, Qt::NoBrush, QPainter::CompositionMode_Source, false);

            prev = cur;
            prevW = curW;
        }
    }

}

// draw a single paint dab at the given location (optimized with direct rendering)
void PenTool::paintAt(QPointF point)
{
    // Safety checks
    if (!mEditor || !mScribbleArea) return;
    
    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;
    
    // Calculate brush size with pressure (respect properties.pressure toggle)
    qreal baseWidth = properties.width;
    if (mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
        const qreal gw = mEditor->tools()->globalWidth();
        if (gw > 0.0) baseWidth = qMax<qreal>(1.0, gw);
    }
    
    baseWidth = qBound(1.0, baseWidth, 200.0);
    // Normalized pressure mapping: pressure 0 → 1px, pressure 1 → baseWidth.
    // Additive formula keeps "light" and "heavy" pressure consistent regardless
    // of base size (no size-relative scaling of the pressure range).
    // Note: mPressureCurveEnabled only controls LUT application (done upstream);
    // mCurrentPressure already has the curve baked in when enabled.
    qreal pressure = properties.pressure
                     ? qBound(0.0, mCurrentPressure, 1.0)
                     : 1.0;
    // Guarantee at least 1px coverage even at extremely low pressure to prevent pixel gaps.
    qreal brushWidth = properties.pressure
                           ? qMax<qreal>(1.0, 1.0 + (baseWidth - 1.0) * pressure)
                           : qMax<qreal>(1.0, baseWidth);
    mCurrentWidth = brushWidth;

    // Overdap avoidance: only skip re-stamping the same logical cell when the dab is clearly > 1px.
    // For ~1px dabs, skipping can create visible gaps due to rounding/quantization.
    if (brushWidth > 1.5) {
        qint32 cx = qRound(point.x());
        qint32 cy = qRound(point.y());
        quint64 cellKey = (quint64(quint32(cx)) << 32) | quint32(cy);
        if (mVisitedDabCells.contains(cellKey)) {
            return;
        }
        mVisitedDabCells.insert(cellKey);
    }
    
    QColor strokeColor = (mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                               ? mEditor->tools()->strokeColorOverride()
                               : mEditor->color()->frontColor();
    strokeColor.setAlpha(255);  // Pen tool never uses alpha - always fully opaque
    
    // Antialiasing disabled for Pen tool to avoid blending artifacts
    bool antialiasing = false;
    
    // Direct tile painting (fastest for solid circles, gapless strokes)
    mScribbleArea->drawPen(point, brushWidth, strokeColor, antialiasing);

    // Track dabs for realtime flush throttling.
    ++mDabsSinceRealtimeFlush;
    
    // Record dab for post-correction restamping
    // We store cumulative distance along the stroke and the actual stamped width
    if (properties.postCorrectionLevel > 0) {
        qreal seg = 0.0;
        if (!mRecordedDabWidths.isEmpty()) {
            seg = QLineF(getLastProcessedPoint(), point).length();
        }
        mRecordedCum += seg;
        mRecordedDabCumDist.push_back(mRecordedCum);
        mRecordedDabWidths.append(brushWidth);
    }

    setLastProcessedPoint(point);
}

void PenTool::drawStroke()
{
    // Safety checks
    if (!mEditor || !mScribbleArea) return;
    
    StrokeTool::drawStroke();

    Layer* layer = mEditor->layers()->currentLayer();
    if (!layer) {
        return;
    }
    if (layer->type() == Layer::BITMAP)
    {
        const QPointF currentPoint = getCurrentPoint();

        // Validate coordinates
        if (qAbs(currentPoint.x()) > 100000.0 || qAbs(currentPoint.y()) > 100000.0) {
            return;
        }

        // Continuous segment rasterization (gapless): draw a round-capped stroke
        // between the last point and the current point.
        const QPointF segStart = mLastBrushPoint;
        const qreal segmentLength = QLineF(segStart, currentPoint).length();

        // Resolve base width early so the teleport guard can use it.
        qreal baseWidth = properties.width;
        if (mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
            const qreal gw = mEditor->tools()->globalWidth();
            if (gw > 0.0) baseWidth = qMax<qreal>(1.0, gw);
        }
        baseWidth = qBound<qreal>(1.0, baseWidth, 200.0);

        // Guard against "teleport" artifacts: during fast circular motion the
        // smoothing/stabilizer output can lag far behind the real cursor, causing
        // a segment to bridge across the interior of an arc (the "webbing" effect).
        // If the segment is implausibly long for a single input event, skip drawing
        // it but still advance mLastBrushPoint so the next segment starts correctly.
        // Threshold: 200px or 20× brush width, whichever is larger.
        const qreal maxSegmentLength = qMax(200.0, baseWidth * 20.0);
        if (segmentLength > maxSegmentLength) {
            mLastBrushPoint = currentPoint;
            mStrokePoints.append(currentPoint);
            mStrokePressures.append(mCurrentPressure);
            return;
        }

        // Update pressure once per segment (cheap, stable). We still record stroke
        // points and pressures for smoothing/hybrid features.
        if (properties.pressure) {
            const qreal velocity = qBound(0.0, (mEditor && mEditor->view() && mEditor->view()->scaling() > 0.0)
                                                ? (mInterpolator.getVelocity() / mEditor->view()->scaling())
                                                : mInterpolator.getVelocity(),
                                          10000.0);
            const QPointF delta = currentPoint - mLastPipelinePoint;
            const qreal pipelinePressure = mPressurePipeline.sample(mLastRawPressure,
                                                                    velocity,
                                                                    mPipelineDistance,
                                                                    delta);
            // Quantize to 256 buckets so the LUT is only re-evaluated when pressure
            // changes meaningfully, reducing noise from high-frequency tablet jitter.
            const int bucket = qBound(0, int(qRound(pipelinePressure * 256.0)), 256);
            const qreal qPressure = qreal(bucket) / 256.0;
            mCurrentPressure = mPressureCurveEnabled ? mPressureCurveLUT.evaluate(qPressure)
                                                     : qPressure;
            mLastPipelinePoint = currentPoint;
        }

        // (baseWidth already resolved above for the teleport guard)
        // Normalized pressure mapping: pressure 0 → 1px, pressure 1 → baseWidth.
        // Additive formula keeps "light" and "heavy" pressure consistent regardless
        // of base size (no size-relative scaling of the pressure range).
        // Note: mPressureCurveEnabled only controls LUT application (done above);
        // mCurrentPressure already has the curve baked in when enabled.
        const qreal pressure = properties.pressure
                                   ? qBound<qreal>(0.0, mCurrentPressure, 1.0)
                                   : 1.0;
        const qreal brushWidth = properties.pressure
                                     ? qMax<qreal>(1.0, 1.0 + (baseWidth - 1.0) * pressure)
                                     : qMax<qreal>(1.0, baseWidth);
        mCurrentWidth = brushWidth;

        QColor strokeColor = (mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                                 ? mEditor->tools()->strokeColorOverride()
                                 : mEditor->color()->frontColor();
        strokeColor.setAlpha(255);

        if (segmentLength < 0.01) {
            // Tap or stationary: still stamp a single dab.
            paintAt(currentPoint);
        } else {
            // Draw capsule segment in canvas space.
            QPainterPath path(segStart);
            path.lineTo(currentPoint);

            QPen pen(strokeColor);
            pen.setWidthF(brushWidth);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);

            // Bitmap Pen strokes must remain strictly pixelated (no anti-aliasing).
            mScribbleArea->drawPathCanvas(path, pen, Qt::NoBrush, QPainter::CompositionMode_Source, false);

            // Track as one logical emitted segment for realtime flush throttling.
            ++mDabsSinceRealtimeFlush;

            // Record samples for post-correction restamping.
            // Previously we only recorded the initial press dab (paintAt), which caused
            // post-correction to clear the buffer and restamp only a handful of dots.
            if (properties.postCorrectionLevel > 0) {
                qreal seg = 0.0;
                if (!mRecordedDabWidths.isEmpty()) {
                    seg = QLineF(getLastProcessedPoint(), currentPoint).length();
                }
                mRecordedCum += seg;
                mRecordedDabCumDist.push_back(mRecordedCum);
                mRecordedDabWidths.append(brushWidth);
            }

            setLastProcessedPoint(currentPoint);
        }

        mStrokePoints.append(currentPoint);
        mStrokePressures.append(mCurrentPressure);

        mLastBrushPoint = currentPoint;
    }
    else if (layer->type() == Layer::VECTOR)
    {
        // Vector layer uses interpolated stroke points
        QList<QPointF> p = mInterpolator.interpolateStroke();
        
        const qreal raw = mCurrentPressure;
        qreal pressure = (properties.pressure && mPressureCurveEnabled)
                         ? qBound<qreal>(0.05, raw, 1.0)
                         : 1.0;
        qreal baseWidth = properties.width;
        if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
            const qreal gw = mEditor->tools()->globalWidth();
            if (gw > 0.0) {
                baseWidth = qMax<qreal>(1.0, gw);
            }
        }
        qreal brushWidth = baseWidth * pressure;

        QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                                   ? mEditor->tools()->strokeColorOverride()
                                   : mEditor->color()->frontColor();
        strokeColor.setAlpha(255);  // Pen tool never uses alpha - always fully opaque
        QPen pen(strokeColor,
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

    // Ensure the most recent pressure sample used for smoothing/post-correction is valid
    if (mPressureCurveEnabled && !mStrokePressures.isEmpty()) {
        qreal pr = mCurrentPressure;
        if (pr <= 0.0) pr = 1.0;
        mStrokePressures.last() = qBound<qreal>(0.0, pr, 1.0);
    }
}

void PenTool::paintVectorStroke(Layer* layer)
{
    if (mStrokePoints.empty())
        return;

    // Clear the temporary pixel path
    mScribbleArea->clearDrawingBuffer();
    qreal tol = mScribbleArea->getCurveSmoothing() / mEditor->view()->scaling();

    BezierCurve curve(mStrokePoints, mStrokePressures, tol);
    curve.setWidth(properties.width);
    curve.setFeather(properties.feather);
    curve.setFilled(false);
    curve.setInvisibility(properties.invisibility);
    curve.setVariableWidth(properties.pressure);
    // Use stroke color override if active; otherwise use current palette index
    int colorIndex = mEditor->color()->frontColorNumber();
    if (mEditor->tools() && mEditor->tools()->hasStrokeColorOverride()) {
        const QColor override = mEditor->tools()->strokeColorOverride();
        Object* obj = mEditor->object();
        if (obj) {
            int found = -1;
            const int count = obj->getColorCount();
            for (int i = 0; i < count; ++i) {
                if (obj->getColor(i).color == override) { found = i; break; }
            }
            if (found < 0) {
                // Append override color to the palette and use it
                obj->addColor(ColorRef(override, ColorRef::getDefaultColorName(override)));
                colorIndex = obj->getColorCount() - 1;
            } else {
                colorIndex = found;
            }
        }
    }
    curve.setColorNumber(colorIndex);

    auto pLayerVector = static_cast<LayerVector*>(layer);
    VectorImage* vectorImage = pLayerVector->getLastVectorImageAtFrame(mEditor->currentFrame(), 0);
    if (vectorImage == nullptr) { return; } // Can happen if the first frame is deleted while drawing
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

    // Request raster re-render from newly added vector stroke
    if (mScribbleArea) {
        mScribbleArea->hybridStack()->rerenderRasterFromVectors(mEditor->currentFrame());
    }
}

// === OPTIMIZED RENDERING PIPELINE ===

qreal PenTool::calculateAdaptiveSpacing(qreal velocity, qreal curvature)
{
    // AA-aware spacing fraction of brush diameter
    const bool aa = (properties.useAA == ON);

    // Normalize velocity (canvas px/s) into [0,1]
    const qreal slowThreshold = 50.0;   // steady hand
    const qreal fastThreshold = 900.0;  // very fast swipe
    qreal v = 0.0;
    if (velocity <= slowThreshold) {
        v = 0.0;
    } else if (velocity >= fastThreshold) {
        v = 1.0;
    } else {
        v = (velocity - slowThreshold) / (fastThreshold - slowThreshold);
    }

    // Base fractions chosen to avoid gaps while keeping dab count low (optimized for performance)
    const qreal baseFrac = aa ? 0.42 : 0.34;        // at slow speed (slightly relaxed from 0.40/0.32)
    const qreal extraFrac = aa ? 0.08 : 0.06;       // additional spacing at high speed

    // Curvature tightening (reduce spacing up to 40% on sharp turns - less aggressive for performance)
    qreal curvatureMult = 1.0 - qBound<qreal>(0.0, curvature, 1.0) * 0.4;

    qreal fraction = (baseFrac + extraFrac * v) * curvatureMult;

    // Hard safety clamps to guarantee continuous coverage
    const qreal minFrac = aa ? 0.32 : 0.26;  // Slightly higher floor for performance
    const qreal maxFrac = aa ? 0.52 : 0.44;  // Slightly higher ceiling for high-speed performance
    fraction = qBound(minFrac, fraction, maxFrac);

    return fraction;  // caller multiplies by brush diameter in screen-space
}

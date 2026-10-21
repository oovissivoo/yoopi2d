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

#ifndef PENTOOL_H
#define PENTOOL_H

#include "stroketool.h"
#include "pressurepipeline.h"
#include "pressurecurvelut.h"
#include <QVector>
#include <QSet>
#include <QtGlobal>

class Layer;

class PenTool : public StrokeTool
{
    Q_OBJECT
public:
    PenTool(QObject* parent = 0);
    ToolType type() override { return PEN; }
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;
    void resetToDefault() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void drawStroke();
    void paintAt(QPointF point);
    void paintVectorStroke(Layer *layer);

    void setWidth(const qreal width) override;
    void setPressure(const bool pressure) override;
    void setAA(const int AA) override;
    void setStabilizerLevel(const int level) override;
    // Post-stroke correction level: 0..3
    void setPostCorrectionLevel(int level) { properties.postCorrectionLevel = qBound(0, level, 3); }

    // Per-pen pressure curve API (multi-point)
    void setPressureCurve(const QVector<QPointF>& points);
    QVector<QPointF> getPressureCurve() const;
    bool isPressureCurveEnabled() const { return mPressureCurveEnabled; }
    void setPressureCurveEnabled(bool enabled) { mPressureCurveEnabled = enabled; }

    // Stabilizer mode cursor position getter
    QPointF getStabilizerClampedCursorPos() const { return mStabilizerClampedCursorPos; }
    
private:
    // Calculate adaptive spacing based on velocity and curvature
    qreal calculateAdaptiveSpacing(qreal velocity, qreal curvature);

    void maybeFlushRealtime(quint64 eventTimestampMs);

    QPointF mLastBrushPoint;
    QPointF mMouseDownPoint;
    QPointF mLastDabPoint;  // Last stamped dab position for spacing

    // Pressure curve control points (unit space)
    QVector<QPointF> mPressureCurvePoints;
    PressureCurveLUT mPressureCurveLUT;
    bool mPressureCurveEnabled { true };
    
    // Stroke state
    qreal mAccumulatedDistance = 0.0;
    QList<QPointF> mStrokePoints;
    QList<qreal> mStrokePressures;
    qreal mCurrentPressure = 1.0;
    QSet<quint64> mVisitedDabCells;

    // Stabilizer mode cursor tracking
    QPointF mStabilizerClampedCursorPos { 0, 0 };
    qreal mStabilizerPrevDistance { 0.0 };
    bool mStabilizerCursorActive { false };
    // Post-stroke correction helper for bitmap strokes
    void applyPostStrokeCorrectionBitmap();

    // Recorded dabs during drawing (for path-retargeted post-correction)
    QList<qreal> mRecordedDabWidths;   // width per dab as actually stamped
    QVector<qreal> mRecordedDabCumDist; // cumulative distance along the original stroke per dab
    qreal mRecordedCum = 0.0;
    bool mUndoMacroActive = false;

    // Pressure pipeline (always-on pressure smoothing and tapering)
    PressurePipeline mPressurePipeline;
    qreal mLastRawPressure { 0.5 };
    QPointF mLastPipelinePoint;  // Track position for distance calculation in pipeline
    qreal mPipelineDistance;     // Accumulated distance for pipeline
    // Adaptive dab spacing: smoothed multiplier from previous segment
    qreal mLastSpacingMult = 0.5;

    // Final stroke geometry used for bitmap hybrid vector recording
    QVector<QPointF> mFinalStrokePoints;
    qreal mFinalStrokeBaseWidth = 0.0;

    // Realtime flush throttling for bitmap pen (keeps delayed stabilizer preview smooth)
    quint64 mLastRealtimeFlushTimestampMs { 0 };
    int mDabsSinceRealtimeFlush { 0 };
};

#endif // PENTOOL_H

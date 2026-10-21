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

#ifndef STROKEINTERPOLATOR_H
#define STROKEINTERPOLATOR_H

#include <QQueue>
#include <QPointF>
#include <QList>
#include <QElapsedTimer>
#include <QTimer>

#include "pencildef.h"


class PointerEvent;

class StrokeInterpolator : public QObject
{
public:
    StrokeInterpolator();

    void pointerPressEvent(PointerEvent* event);
    void pointerMoveEvent(PointerEvent* event);
    void pointerReleaseEvent(PointerEvent* event);
    void setPressure(float pressure);
    void setStabilizerLevel(int level);
    
    // Krita-like smoothing configuration
    void setPenSmoothingMode(PenSmoothingMode mode) { mPenSmoothingMode = mode; }
    // Weighted
    void setWeightedDistance(qreal d) { mWeightedDistance = d; }
    void setWeightedStrokeEnding(qreal s) { mWeightedStrokeEnding = s; }
    void setWeightedSmoothPressure(bool b) { mWeightedSmoothPressure = b; }
    void setWeightedScalableDistance(bool b) { mWeightedScalableDistance = b; }
    // Lazy Mouse (Stabilizer mode)
    void setStab2Distance(qreal d) { mStab2Distance = d; }  // Legacy - unused in lazy mouse
    void setStab2Delay(qreal d) { mStab2Delay = d; }  // Lazy mouse leash distance in pixels
    void setStab2FinishLine(bool b) { mStab2FinishLine = b; }  // Legacy - unused
    void setStab2StabilizeSensors(bool b) { mStab2StabilizeSensors = b; }  // Legacy - unused
    void setStab2ScalableDistance(bool b) { mStab2ScalableDistance = b; }  // Legacy - unused

    float getPressure() { return mTabletPressure; }
    int getStabilizerLevel() { return mStabilizerLevel; }
    bool isActive() const { return mStrokeStarted; }

    QList<QPointF> interpolateStroke();
    void interpolatePoll();
    QPointF interpolateStart(QPointF firstPoint);
    void interpolatePollAndPaint();
    void interpolateEnd();
    void smoothMousePos(QPointF pos);
    QList<QPointF> meanInpolOp(QList<QPointF> points, qreal x, qreal y, qreal pressure);
    QList<QPointF> noInpolOp(QList<QPointF> points);
    QList<QPointF> tangentInpolOp(QList<QPointF> points);

    QPointF getCurrentPixel() const { return mCurrentPixel; }
    QPointF getLastPixel() const { return mLastPixel; }
    QPointF getLastMeanPixel() const { return mLastInterpolated; }
    QPointF getCurrentPressPixel() const { return mCurrentPressPixel; }
    QPointF getLastRawPixel() const { return mLastRaw; }

    // Velocity tracking
    qreal getVelocity() const { return mCurrentVelocity; }

    // Overlay helpers for Lazy Mouse (Stabilizer mode)
    QPointF getStab2AnchorPixel() const { return mStab2Anchor; }  // Drawing anchor position
    qreal getStab2Delay() const { return mStab2Delay; }  // Leash distance in pixels

private:
    static const int STROKE_QUEUE_LENGTH = 3; // 4 points for cubic bezier

    void reset();

    // Returns a monotonic timestamp in milliseconds for velocity tracking.
    // Uses PointerEvent::timestamp() when available, otherwise falls back
    // to an internal QElapsedTimer.
    quint64 effectiveTimestampMs(const PointerEvent* event);

    float pressure = 1.0f; // last pressure
    QQueue<QPointF> strokeQueue;
    QQueue<qreal> pressureQueue;

    QTimer timer;

    QPointF mCurrentPressPixel = { 0, 0 };
    QPointF mCurrentPixel = { 0, 0 };
    QPointF mLastPixel = { 0, 0 };
    QPointF mLastInterpolated = { 0, 0 };

    QPointF m_previousTangent;
    bool    mHasTangent = false;
    bool    mStrokeStarted = false;
    bool    mTabletInUse = false;
    float   mTabletPressure = 1.f;
    int     mStabilizerLevel = 0;

    // Krita-like smoothing state
    PenSmoothingMode mPenSmoothingMode = PenSmoothingMode::LEGACY;
    // Weighted
    qreal mWeightedDistance = 12.0;
    qreal mWeightedStrokeEnding = 0.0;
    bool  mWeightedSmoothPressure = false;
    bool  mWeightedScalableDistance = true;
    QPointF mWeightedOut = {0,0};
    QPointF mLastRaw = {0,0};
    // Lazy Mouse (Stabilizer mode)
    qreal mStab2Distance = 16.0;  // Legacy - unused in lazy mouse
    qreal mStab2Delay = 4.0;  // Leash distance in pixels
    bool  mStab2FinishLine = true;  // Legacy - unused
    bool  mStab2StabilizeSensors = false;  // Legacy - unused
    bool  mStab2ScalableDistance = true;  // Legacy - unused
    QPointF mStab2Anchor = {0,0};  // Drawing anchor position (follows cursor at leash distance)

    // Velocity tracking (circular buffer approach inspired by Krita's KisSpeedSmoother)
    struct VelocitySample {
        QPointF position;
        quint64 timestamp;
        qreal distance;
    };
    QList<VelocitySample> mVelocityBuffer;
    int mVelocityBufferSize = 8;
    qreal mCurrentVelocity = 0.0;
    qreal mRawVelocity = 0.0;
    QPointF mLastVelocityPoint = {0,0};
    quint64 mLastVelocityTimestamp = 0;

    // Fallback timestamp source for platforms/event paths where PointerEvent::timestamp()
    // is 0. Used only for velocity tracking.
    QElapsedTimer mFallbackTimer;
    bool mFallbackTimerStarted = false;
};

#endif // STROKEINTERPOLATOR_H

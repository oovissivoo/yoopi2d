/***************************************************************************
 * This code is heavily influenced by the instrument proxy from QAquarelle *
 * QAquarelle -   Copyright (C) 2009 by Anton R. <commanderkyle@gmail.com> *
 *                                                                         *
 *   QAquarelle is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "strokeinterpolator.h"

#include <QLineF>
#include <QPainterPath>
#include "object.h"
#include "pointerevent.h"

StrokeInterpolator::StrokeInterpolator()
{
    mTabletInUse = false;
    mTabletPressure = 0;

    reset();
    connect(&timer, &QTimer::timeout, this, &StrokeInterpolator::interpolatePollAndPaint);
}

void StrokeInterpolator::reset()
{
    mStrokeStarted = false;
    pressureQueue.clear();
    strokeQueue.clear();
    pressure = 0.0f;
    mHasTangent = false;
    timer.stop();
    mStabilizerLevel = -1;
    
    // Initialize velocity tracking
    mVelocityBuffer.clear();
    mVelocityBufferSize = 8;
    mCurrentVelocity = 0.0;
    mRawVelocity = 0.0;
    mLastVelocityPoint = QPointF(0, 0);
    mLastVelocityTimestamp = 0;
}

quint64 StrokeInterpolator::effectiveTimestampMs(const PointerEvent* event)
{
    if (!event) {
        return 0;
    }

    const quint64 ts = event->timestamp();
    if (ts != 0) {
        return ts;
    }

    if (!mFallbackTimerStarted) {
        mFallbackTimer.start();
        mFallbackTimerStarted = true;
    }
    return static_cast<quint64>(mFallbackTimer.elapsed());
}

void StrokeInterpolator::setPressure(float pressure)
{
    mTabletPressure = pressure;
}

void StrokeInterpolator::pointerPressEvent(PointerEvent* event)
{
    reset();
    if (!(event->button() == Qt::NoButton)) // if the user is pressing the left/right button
    {
        mCurrentPressPixel = event->viewportPos();
    }

    mLastPixel = mCurrentPixel = event->viewportPos();

    // Seed added smoothing modes so stroke starts exactly at press point
    mStab2Anchor = mCurrentPixel;
    mWeightedOut = mCurrentPixel;
    mLastRaw = mCurrentPixel;

    mStrokeStarted = true;
    setPressure(event->pressure());

    mTabletInUse = mTabletInUse || event->isTabletEvent();
    
    // Initialize velocity tracking with first sample
    mLastVelocityPoint = event->viewportPos();
    mLastVelocityTimestamp = effectiveTimestampMs(event);
    VelocitySample sample;
    sample.position = mLastVelocityPoint;
    sample.timestamp = mLastVelocityTimestamp;
    sample.distance = 0.0;
    mVelocityBuffer.append(sample);
    mCurrentVelocity = 0.0;
    mRawVelocity = 0.0;
}

void StrokeInterpolator::pointerMoveEvent(PointerEvent* event)
{
    const QPointF raw = event->viewportPos();
    mLastRaw = raw;
    
    // Calculate velocity using circular buffer approach (inspired by Krita's KisSpeedSmoother)
    QPointF currentPos = event->viewportPos();
    quint64 currentTime = effectiveTimestampMs(event);
    QPointF deltaPos = currentPos - mLastVelocityPoint;
    qreal distance = QLineF(QPointF(), deltaPos).length();
    quint64 deltaTime = currentTime - mLastVelocityTimestamp;
    
    // Calculate raw velocity
    if (deltaTime > 0) {
        mRawVelocity = (distance / deltaTime) * 1000.0; // Convert ms to seconds
    }
    
    // Add to circular buffer
    VelocitySample sample;
    sample.position = currentPos;
    sample.timestamp = currentTime;
    sample.distance = distance;
    mVelocityBuffer.append(sample);
    
    // Maintain buffer size
    if (mVelocityBuffer.size() > mVelocityBufferSize) {
        mVelocityBuffer.removeFirst();
    }
    
    // Smooth velocity using rolling window (last 3-5 samples)
    qreal totalDistance = 0.0;
    quint64 totalTime = 0;
    int windowSize = qMin(5, mVelocityBuffer.size());
    for (int i = mVelocityBuffer.size() - windowSize; i < mVelocityBuffer.size(); i++) {
        if (i > 0) {
            totalDistance += mVelocityBuffer[i].distance;
            quint64 dt = mVelocityBuffer[i].timestamp - mVelocityBuffer[i-1].timestamp;
            totalTime += dt;
        }
    }
    
    if (totalTime > 0) {
        mCurrentVelocity = (totalDistance / totalTime) * 1000.0; // Convert ms to seconds
    }
    
    // Update tracking
    mLastVelocityPoint = currentPos;
    mLastVelocityTimestamp = currentTime;

    if (!mStrokeStarted) {
        // Hover/idle: always follow raw cursor (legacy behavior)
        mLastPixel = mCurrentPixel;
        mCurrentPixel = raw;
        mLastInterpolated = mCurrentPixel;
        if (event->isTabletEvent()) setPressure(event->pressure());
        return;
    }

    // While drawing: apply existing stabilizer or krita-like mode
    if (mStabilizerLevel != -1 && mPenSmoothingMode == PenSmoothingMode::LEGACY) {
        smoothMousePos(raw);
    } else {
        mLastPixel = mCurrentPixel;
        mCurrentPixel = raw;
        mLastInterpolated = mCurrentPixel;
    }

    // Krita-like smoothing modes (active only during stroke)
    if (mPenSmoothingMode == PenSmoothingMode::WEIGHTED) {
        qreal d = qMax<qreal>(1.0, mWeightedDistance);
        // Treat distance == 0 as OFF: pass raw coordinates through
        if (mWeightedDistance <= 0.0) {
            mLastPixel = mCurrentPixel = raw;
            mLastInterpolated = mCurrentPixel;
        } else {
        if (mWeightedScalableDistance) {
            // Future: scale with view if needed. Left as is without view dep.
        }
        const qreal alpha = qBound<qreal>(0.02, 1.0 / d, 0.8);
        // Gate first dab: require min travel equal to distance before output starts to move
        // Keep output steady until threshold is reached
        mWeightedOut = mWeightedOut + alpha * (raw - mWeightedOut);
        mCurrentPixel = mWeightedOut;
        mLastPixel = mCurrentPixel;
        mLastInterpolated = mCurrentPixel;
        if (mWeightedSmoothPressure && event->isTabletEvent()) {
            // Simple exponential smoothing on pressure mirroring distance alpha
            mTabletPressure = static_cast<float>((1.0 - alpha) * mTabletPressure + alpha * event->pressure());
        }
        }
    } else if (mPenSmoothingMode == PenSmoothingMode::PULL) {
        // Lazy Mouse implementation: anchor follows cursor when it exceeds leash distance
        // mStab2Delay controls the leash/distance (converted to pixels)
        const qreal leashDistance = qMax<qreal>(1.0, mStab2Delay);
        const qreal toCursor = QLineF(mStab2Anchor, raw).length();
        
        if (toCursor > leashDistance) {
            // Cursor exceeded leash - move anchor to maintain leash distance
            // Calculate direction from anchor to cursor
            QLineF leashLine(mStab2Anchor, raw);
            leashLine.setLength(toCursor - leashDistance);
            mStab2Anchor = mStab2Anchor + QPointF(leashLine.dx(), leashLine.dy());
        }
        // Anchor stays within leash distance - cursor position is the anchor
        mLastPixel = mCurrentPixel;
        mCurrentPixel = mStab2Anchor;
        mLastInterpolated = mCurrentPixel;
    }

    if(event->isTabletEvent()) {
        setPressure(event->pressure());
    }
}

void StrokeInterpolator::pointerReleaseEvent(PointerEvent* event)
{
    // flush out stroke
    if (mStrokeStarted)
    {
        pointerMoveEvent(event);
    }

    mStrokeStarted = false;
    mTabletInUse = mTabletInUse && !event->isTabletEvent();
}

void StrokeInterpolator::setStabilizerLevel(int level)
{
    mStabilizerLevel = level;
}

void StrokeInterpolator::smoothMousePos(QPointF pos)
{
    // Smooth mouse position before drawing
    QPointF smoothPos;

    if (mStabilizerLevel == StabilizationLevel::NONE)
    {
        mLastPixel = mCurrentPixel;
        mCurrentPixel = pos;
        mLastInterpolated = mCurrentPixel;
    }
    else if (mStabilizerLevel == StabilizationLevel::SIMPLE)
    {
        // simple interpolation
        smoothPos = QPointF((pos.x() + mCurrentPixel.x()) / 2.0, (pos.y() + mCurrentPixel.y()) / 2.0);
        mLastPixel = mCurrentPixel;
        mCurrentPixel = smoothPos;
        mLastInterpolated = mCurrentPixel;

        // shift queue
        while (strokeQueue.size() >= STROKE_QUEUE_LENGTH)
        {
            strokeQueue.pop_front();
        }

        strokeQueue.push_back(smoothPos);
    }
    else if (mStabilizerLevel == StabilizationLevel::STRONG)
    {
        smoothPos = QPointF((pos.x() + mLastInterpolated.x()) / 2.0, (pos.y() + mLastInterpolated.y()) / 2.0);

        mLastInterpolated = mCurrentPixel;
        mCurrentPixel = smoothPos;
        mLastPixel = mLastInterpolated;
    }

    if (!mStrokeStarted)
    {
        // Ensure idle behaves like legacy
        mLastPixel = mCurrentPixel;
        mCurrentPixel = pos;
        mLastInterpolated = mCurrentPixel;
        return;
    }

    if (!mTabletInUse)   // a mouse is used instead of a tablet
    {
        setPressure(1.0);
    }
}


QPointF StrokeInterpolator::interpolateStart(QPointF firstPoint)
{
    if (mStabilizerLevel == StabilizationLevel::SIMPLE)
    {
        // Clear queue
        strokeQueue.clear();
        pressureQueue.clear();

        mLastPixel = firstPoint;
    }
    else if (mStabilizerLevel == StabilizationLevel::STRONG)
    {
        // Clear queue
        strokeQueue.clear();
        pressureQueue.clear();

        const int sampleSize = 5;
        Q_ASSERT(sampleSize > 0);

        // fill strokeQueue with firstPoint x times
        for (int i = sampleSize; i > 0; i--)
        {
            strokeQueue.enqueue(firstPoint);
        }

        // last interpolated stroke should always be firstPoint
        mLastInterpolated = firstPoint;

        // draw and poll each millisecond
        timer.setInterval(sampleSize);
        timer.start();
    }
    else if (mStabilizerLevel == StabilizationLevel::NONE)
    {
        // Clear queue
        strokeQueue.clear();
        pressureQueue.clear();

        mLastPixel = firstPoint;
    }
    return firstPoint;
}

void StrokeInterpolator::interpolatePoll()
{
    // remove oldest stroke
    strokeQueue.dequeue();

    // add new stroke with the last interpolated pixel position
    strokeQueue.enqueue(mLastInterpolated);
}

void StrokeInterpolator::interpolatePollAndPaint()
{
    //qDebug() <<"inpol:" << mStabilizerLevel << "strokes"<< strokeQueue;
    if (!strokeQueue.isEmpty())
    {
        interpolatePoll();
        interpolateStroke();
    }
}

QList<QPointF> StrokeInterpolator::interpolateStroke()
{
    // is nan initially
    QList<QPointF> result;

    if (mStabilizerLevel == StabilizationLevel::SIMPLE)
    {
        result = tangentInpolOp(result);

    }
    else if (mStabilizerLevel == StabilizationLevel::STRONG)
    {
        qreal x = 0;
        qreal y = 0;
        qreal pressure = 0;
        result = meanInpolOp(result, x, y, pressure);

    }
    else if (mStabilizerLevel == StabilizationLevel::NONE)
    {
        result = noInpolOp(result);
    }
    return result;
}

QList<QPointF> StrokeInterpolator::noInpolOp(QList<QPointF> points)
{
    setPressure(getPressure());

    points << mLastPixel << mLastPixel << mCurrentPixel << mCurrentPixel;

    // Set lastPixel to CurrentPixel
    // new interpolated pixel
    mLastPixel = mCurrentPixel;

    return points;
}

QList<QPointF> StrokeInterpolator::tangentInpolOp(QList<QPointF> points)
{
    static const qreal smoothness = 1.f;
    QLineF line(mLastPixel, mCurrentPixel);

    qreal scaleFactor = line.length() * 3.f;

    if (!mHasTangent && scaleFactor > 0.01f)
    {
        mHasTangent = true;
        /*
        qDebug() << "scaleFactor" << scaleFactor
                 << "current pixel " << mCurrentPixel
                 << "last pixel" << mLastPixel;
         */
        m_previousTangent = (mCurrentPixel - mLastPixel) * smoothness / (3.0 * scaleFactor);
        //qDebug() << "previous tangent" << m_previousTangent;
        QLineF _line(QPointF(0, 0), m_previousTangent);
        // don't bother for small tangents, as they can induce single pixel wobbliness
        if (_line.length() < 2)
        {
            m_previousTangent = QPointF(0, 0);
        }
    }
    else
    {
        QPointF c1 = mLastPixel + m_previousTangent * scaleFactor;
        QPointF newTangent = (mCurrentPixel - c1) * smoothness / (3.0 * scaleFactor);
        //qDebug() << "scalefactor1=" << scaleFactor << m_previousTangent << newTangent;
        if (scaleFactor == 0)
        {
            newTangent = QPointF(0, 0);
        }
        else
        {
            //QLineF _line(QPointF(0,0), newTangent);
            //if (_line.length() < 2)
            //{
            //    newTangent = QPointF(0,0);
            //}
        }
        QPointF c2 = mCurrentPixel - newTangent * scaleFactor;
        //c1 = mLastPixel;
        //c2 = mCurrentPixel;
        points << mLastPixel << c1 << c2 << mCurrentPixel;
        //qDebug() << mLastPixel << c1 << c2 << mCurrentPixel;
        m_previousTangent = newTangent;
    }

    return points;
}

// Mean sampling interpolation operation
QList<QPointF> StrokeInterpolator::meanInpolOp(QList<QPointF> points, qreal x, qreal y, qreal pressure)
{
    for (int i = 0; i < strokeQueue.size(); i++)
    {
        x += strokeQueue[i].x();
        y += strokeQueue[i].y();
        pressure += getPressure();
    }

    // get arithmetic mean of x, y and pressure
    x /= strokeQueue.size();
    y /= strokeQueue.size();
    pressure /= strokeQueue.size();

    // Use our interpolated points
    QPointF mNewInterpolated(x, y);

    points << mLastPixel << mLastInterpolated << mNewInterpolated << mCurrentPixel;

    // Set lastPixel non interpolated pixel to our
    // new interpolated pixel
    mLastPixel = mNewInterpolated;

    return points;
}

void StrokeInterpolator::interpolateEnd()
{
    // Stop timer
    timer.stop();
    if (mStabilizerLevel == StabilizationLevel::STRONG)
    {
        if (!strokeQueue.isEmpty())
        {
            int sampleSize = 5;
            Q_ASSERT(sampleSize > 0);
            for (int i = sampleSize; i > 0; i--)
            {
                interpolatePoll();
                interpolateStroke();
            }
        }
    }
    // Weighted stroke ending: gently chase raw position a few extra steps
    if (mPenSmoothingMode == PenSmoothingMode::WEIGHTED && mWeightedStrokeEnding > 0.0)
    {
        const int steps = qBound(1, (int)qRound(12 * mWeightedStrokeEnding), 48);
        const qreal d = qMax<qreal>(1.0, mWeightedDistance);
        const qreal alpha = qBound<qreal>(0.02, 1.0 / d, 0.8);
        for (int i = 0; i < steps; ++i)
        {
            QPointF target = mLastRaw; // chase the last raw position
            mWeightedOut = mWeightedOut + alpha * (target - mWeightedOut);
            mCurrentPixel = mWeightedOut;
            mLastInterpolated = mCurrentPixel;
            interpolateStroke();
        }
    }
    // No stabilizer finish line per request
}



#include "pressurepipeline.h"
#include <QtCore/qmath.h>
#include <QtCore/qdebug.h>
#include <QLineF>

PressurePipeline::PressurePipeline(qreal smoothingAmount, qreal taperingStart, qreal taperingEnd)
    : mSmoothingAmount(smoothingAmount)
    , mTaperingStart(taperingStart)
    , mTaperingEnd(taperingEnd)
{
    reset();
}

void PressurePipeline::beginStroke(qreal initialPressure)
{
    mStrokeActive = true;
    mSmoothedPressure = initialPressure;  // Seed EMA with first value
    mStrokeDistance = 0.0;
    mSampleCount = 0;
    mLastVelocity = 0.0;
}

qreal PressurePipeline::sample(qreal rawPressure, qreal velocity, qreal distance, const QPointF& delta)
{
    if (!mStrokeActive) {
        return rawPressure;
    }

    mSampleCount++;
    mStrokeDistance = distance;

    // Pressure Smoothing (always applied)
    // Convert smoothing amount (0-1) to alpha value
    // Keep alpha >= 0.1 for responsiveness, similar to Krita's approach
    qreal alpha = 1.0 - mSmoothingAmount * 0.9;
    alpha = qMax(0.1, qMin(1.0, alpha));
    
    // Apply exponential moving average
    mSmoothedPressure = applyEMA(mSmoothedPressure, rawPressure, alpha);

    qreal processedPressure = mSmoothedPressure;

    // Start Tapering (scale start distance by brush width or explicit override)
    qreal effectiveStartDistance = effectiveStartTaperDistance();
    if (mStrokeDistance < effectiveStartDistance) {
        qreal startTaper = computeStartTaper(mStrokeDistance, effectiveStartDistance);
        qreal taperMultiplier = (1.0 - mTaperingStart) + mTaperingStart * startTaper;
        processedPressure *= taperMultiplier;
    }

    // End Tapering with hysteresis (require consecutive low-velocity samples)
    if (velocity < END_VELOCITY_THRESHOLD && velocity < mLastVelocity * 0.8 && mSampleCount > MIN_SAMPLES_FOR_END_TAPER) {
        mLowVelCount++;
    } else {
        mLowVelCount = 0;
    }
    if (mLowVelCount >= mLowVelThreshold) {
        qreal endTaper = computeEndTaper(velocity);
        qreal taperMultiplier = (1.0 - mTaperingEnd) + mTaperingEnd * endTaper;
        processedPressure *= taperMultiplier;
    }

    // Direction-based tapering: if a delta was provided and indicates a sharp reversal, attenuate more
    if (!delta.isNull()) {
        // Compare direction with last movement (approximate via delta normalized)
        QPointF d = delta;
        qreal len = QLineF(QPointF(), d).length();
        if (len > 0.0) {
            QPointF nd = QPointF(d.x() / len, d.y() / len);
            // Use dot product with previous velocity direction approximated by mLastVelocity (sign only)
            // If last velocity was near zero we skip; otherwise, reduce pressure on sharp reversals
            if (mLastVelocity > 1e-6) {
                // This is a heuristic: penalize sharp reversals by (1 - dot)^directionAggressiveness
                // However we don't have previous direction stored; use velocity sign as weak proxy
                // For now, use reversal when current delta opposes recent movement: dot < 0
                // We'll attenuate by a factor based on direction aggressiveness
                // (Note: this is a no-op if directionAggressiveness == 0)
                Q_UNUSED(nd);
                // Cheap heuristic: if distance jumped backwards relative to stroke progress, apply small attenuation
                // (Left as a conservative no-op because we lack previous normalized direction vector)
            }
        }
    }

    mLastVelocity = velocity;

    // Clamp to avoid zero pressure which can cause rendering issues
    return qMax(0.05, qMin(1.0, processedPressure));
}

qreal PressurePipeline::endStroke()
{
    mStrokeActive = false;
    return mSmoothedPressure;
}

void PressurePipeline::reset()
{
    mSmoothedPressure = 0.5;
    mStrokeDistance = 0.0;
    mSampleCount = 0;
    mLastVelocity = 0.0;
    mStrokeActive = false;
}

void PressurePipeline::setSmoothingAmount(qreal amount)
{
    mSmoothingAmount = qMax(0.0, qMin(1.0, amount));
}

void PressurePipeline::setTaperingStart(qreal start)
{
    mTaperingStart = qMax(0.0, qMin(1.0, start));
}

void PressurePipeline::setTaperingEnd(qreal end)
{
    mTaperingEnd = qMax(0.0, qMin(1.0, end));
}

qreal PressurePipeline::getSmoothingAmount() const
{
    return mSmoothingAmount;
}

qreal PressurePipeline::getTaperingStart() const
{
    return mTaperingStart;
}

qreal PressurePipeline::getTaperingEnd() const
{
    return mTaperingEnd;
}

qreal PressurePipeline::computeStartTaper(qreal distance, qreal effectiveStartDistance) const
{
    // Cubic ease-in for smooth start using effectiveStartDistance
    qreal denom = (effectiveStartDistance > 0.0) ? effectiveStartDistance : START_TAPER_DISTANCE;
    qreal t = distance / denom;
    t = qMax(0.0, qMin(1.0, t));
    return t * t * t;
}

qreal PressurePipeline::computeEndTaper(qreal velocity) const
{
    // Square root ease-out for smooth end
    qreal t = velocity / END_VELOCITY_THRESHOLD;
    t = qMax(0.0, qMin(1.0, t));
    return qSqrt(t);
}

qreal PressurePipeline::applyEMA(qreal current, qreal target, qreal alpha) const
{
    // Standard EMA formula: alpha * target + (1 - alpha) * current
    return alpha * target + (1.0 - alpha) * current;
}

qreal PressurePipeline::effectiveStartTaperDistance() const
{
    if (mExplicitStartTaperDistance > 0.0) return mExplicitStartTaperDistance;
    // Scale base start taper distance by brush width and user scale
    qreal base = START_TAPER_DISTANCE;
    qreal scale = mStartTaperDistanceScale;
    qreal brushFactor = (mBrushWidth > 0.0) ? (mBrushWidth / 12.0) : 1.0; // 12 is nominal brush width
    return base * scale * brushFactor;
}

void PressurePipeline::setStartTaperDistanceScale(qreal scale)
{
    mStartTaperDistanceScale = qMax<qreal>(0.0, scale);
}

void PressurePipeline::setExplicitStartTaperDistance(qreal distance)
{
    mExplicitStartTaperDistance = qMax<qreal>(0.0, distance);
}

void PressurePipeline::setBrushWidth(qreal width)
{
    mBrushWidth = qMax<qreal>(0.0, width);
}

void PressurePipeline::setLowVelThreshold(int threshold)
{
    mLowVelThreshold = qMax(1, threshold);
}

void PressurePipeline::setDirectionAggressiveness(qreal a)
{
    mDirectionAggressiveness = qMax<qreal>(0.0, qMin<qreal>(1.0, a));
}

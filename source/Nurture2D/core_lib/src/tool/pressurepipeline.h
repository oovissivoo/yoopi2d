#ifndef PRESSUREPIPELINE_H
#define PRESSUREPIPELINE_H

#include <QPointF>

/**
 * @brief PressurePipeline provides always-on pressure smoothing and intelligent tapering
 * 
 * This class implements a pressure processing pipeline inspired by Krita's approach:
 * - Always-on exponential moving average (EMA) for temporal pressure smoothing
 * - Velocity-based auto-tapering at stroke start/end with separate controls
 * - Clean lifecycle (begin/sample/end) for integration with drawing tools
 * 
 * The pipeline processes: raw pressure → smoothed (always) → tapered → output
 * The existing pressure curve is applied AFTER the pipeline to preserve current behavior.
 */
class PressurePipeline
{
public:
    /**
     * @brief Construct a new PressurePipeline with default settings
     * @param smoothingAmount Pressure smoothing amount (0.0 = minimal, 1.0 = max)
     * @param taperingStart Start taper strength (0.0 = none, 1.0 = full fade)
     * @param taperingEnd End taper strength (0.0 = none, 1.0 = full fade)
     */
    explicit PressurePipeline(qreal smoothingAmount = 0.3, 
                             qreal taperingStart = 0.5, 
                             qreal taperingEnd = 0.5);

    /**
     * @brief Initialize the pipeline state at stroke start
     * @param initialPressure The initial pressure value to seed the EMA
     */
    void beginStroke(qreal initialPressure);

    /**
     * @brief Process one pressure sample and return smoothed/tapered result
     * @param rawPressure The raw pressure input from the device
     * @param velocity Current velocity in pixels per second
     * @param distance Accumulated distance from stroke start
     * @return Processed pressure value ready for pressure curve application
     */
    qreal sample(qreal rawPressure, qreal velocity, qreal distance, const QPointF& delta = QPointF());

    /**
     * @brief Finalize the stroke and return final pressure value
     * @return The final processed pressure value
     */
    qreal endStroke();

    /**
     * @brief Clear all pipeline state
     */
    void reset();

    // Setters
    void setSmoothingAmount(qreal amount);
    void setTaperingStart(qreal start);
    void setTaperingEnd(qreal end);

    // Getters
    qreal getSmoothingAmount() const;
    qreal getTaperingStart() const;
    qreal getTaperingEnd() const;

private:
    // Configuration
    qreal mSmoothingAmount;    // Controls EMA alpha (0.0 = no smoothing, 1.0 = max smoothing)
    qreal mTaperingStart;      // Controls start fade intensity (0.0 = no taper, 1.0 = full taper)
    qreal mTaperingEnd;        // Controls end fade intensity (tail aggressiveness, 0.0 = no taper, 1.0 = full taper)

    // Pipeline state
    qreal mSmoothedPressure;   // Current EMA state
    qreal mStrokeDistance;     // Accumulated distance traveled during stroke
    qreal mLastVelocity;       // Previous velocity for end taper detection
    int mSampleCount;          // Number of samples processed
    bool mStrokeActive;        // Whether stroke is in progress
    QPointF mLastDirection;    // Last movement direction (normalized)

    // Constants
    static constexpr qreal START_TAPER_DISTANCE = 30.0;      // Base pixels for start tapering (scaled by brush width/scale)
    static constexpr qreal END_VELOCITY_THRESHOLD = 10.0;    // px/sec threshold for end tapering
    static constexpr int MIN_SAMPLES_FOR_END_TAPER = 5;      // Minimum samples before end taper

    // Advanced tunables (mutable via setters)
    qreal mStartTaperDistanceScale = 1.0;    // Multiplier applied to START_TAPER_DISTANCE
    qreal mExplicitStartTaperDistance = 0.0; // If >0, overrides scaled START_TAPER_DISTANCE
    qreal mBrushWidth = 12.0;                // Nominal brush width for scaling (pixels)
    int   mLowVelCount = 0;                  // consecutive low-velocity samples counter
    int   mLowVelThreshold = 2;              // hysteresis threshold (samples) before applying end taper
    qreal mDirectionAggressiveness = 0.5;    // 0..1, extra attenuation on sharp reversals

    // Helper methods
    qreal computeStartTaper(qreal distance, qreal effectiveStartDistance) const;
    qreal computeEndTaper(qreal velocity) const;
    qreal applyEMA(qreal current, qreal target, qreal alpha) const;
    qreal effectiveStartTaperDistance() const;

    // Setters/getters for advanced tunables
    void setStartTaperDistanceScale(qreal scale);
    void setExplicitStartTaperDistance(qreal distance);
    void setBrushWidth(qreal width);
    void setLowVelThreshold(int threshold);
    void setDirectionAggressiveness(qreal a);
    int getLowVelCount() const { return mLowVelCount; }
    int getLowVelThreshold() const { return mLowVelThreshold; }
    qreal getDirectionAggressiveness() const { return mDirectionAggressiveness; }
};

#endif // PRESSUREPIPELINE_H

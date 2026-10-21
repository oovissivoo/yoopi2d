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
#ifndef PRESSURECURVELUT_H
#define PRESSURECURVELUT_H

#include <QVector>
#include <QPointF>
#include <QtMath>

#include "mathutils.h"

/**
 * @brief Lookup-table based evaluator for pressure response curves.
 *
 * This class accelerates evaluation of pressure response curves used by
 * drawing tools by precomputing a fixed-size lookup table (LUT) from the
 * control points. Instead of calling MathUtils::evalPressureCurve() for
 * every pointer event, tools query this LUT with a fast array lookup and
 * linear interpolation.
 *
 * The LUT is lazily rebuilt whenever the control points change, following
 * the same dirty-tracking pattern as CurveGeometryCache. Rebuilding is
 * O(N) in the LUT size (256 samples) and typically performed only when
 * the user edits the curve, while evaluation during drawing is O(1).
 *
 * Typical usage:
 * - On tool configuration change: setCurvePoints()
 * - During drawing: evaluate() per sampled pressure value
 *
 * Performance:
 * - Direct evaluation via MathUtils::evalPressureCurve() involves
 *   sorting, segment search, and spline evaluation (~50-100 cycles).
 * - Using this LUT reduces steady-state cost to a couple of array
 *   accesses and a linear interpolation (~5-10 cycles).
 */
class PressureCurveLUT
{
public:
    /**
     * @brief Constructs a LUT initialized to a linear (identity) curve.
     *
     * The initial mapping is f(x) = x for x in [0, 1], represented by
     * uniformly sampled values in the lookup table.
     */
    PressureCurveLUT()
        : mIsDirty(true)
    {
        // Identity mapping: y = x over [0, 1]
        for (int i = 0; i < LUT_SIZE; ++i) {
            mLookupTable[i] = qreal(i) / qreal(LUT_SIZE - 1);
        }
    }

    /**
     * @brief Sets the control points of the pressure curve.
     *
     * This does not immediately rebuild the LUT; instead it marks the
     * internal state as dirty so the table is lazily recomputed on the
     * next call to evaluate().
     *
     * @param points Control points defining the pressure curve.
     */
    inline void setCurvePoints(const QVector<QPointF> &points)
    {
        mControlPoints = points;
        mIsDirty = true;
    }

    /**
     * @brief Evaluates the pressure curve at the given input using the LUT.
     *
     * If the LUT is marked dirty, it is rebuilt before evaluation.
     * The input is clamped to [0, 1] and evaluated via linear
     * interpolation between the two nearest LUT samples.
     *
     * @param inputPressure Normalized pressure value in [0, 1].
     * @return The corresponding curve output in [0, 1].
     */
    inline qreal evaluate(qreal inputPressure) const
    {
        if (mIsDirty) {
            // Lazy rebuild in const context.
            const_cast<PressureCurveLUT *>(this)->rebuildLUT();
        }

        const qreal x = qBound(0.0, inputPressure, 1.0);
        const qreal fidx = x * qreal(LUT_SIZE - 1);
        const int idx0 = qFloor(fidx);
        const int idx1 = qMin(idx0 + 1, LUT_SIZE - 1);
        const qreal t = fidx - qreal(idx0);

        const qreal y0 = mLookupTable[idx0];
        const qreal y1 = mLookupTable[idx1];

        return y0 + t * (y1 - y0);
    }

    /**
     * @brief Clears the curve and resets to a linear (identity) mapping.
     *
     * This removes all custom control points, marks the LUT as dirty,
     * and repopulates the table with f(x) = x over [0, 1]. The LUT will
     * be considered up-to-date for the identity curve until new control
     * points are set.
     */
    inline void clear()
    {
        mControlPoints.clear();
        mIsDirty = true;

        for (int i = 0; i < LUT_SIZE; ++i) {
            mLookupTable[i] = qreal(i) / qreal(LUT_SIZE - 1);
        }

        mIsDirty = false;
    }

    /**
     * @brief Returns whether any custom control points are set.
     *
     * @return true if no control points have been provided; otherwise false.
     */
    inline bool isEmpty() const
    {
        return mControlPoints.isEmpty();
    }

    /**
     * @brief Gets the current control points defining the pressure curve.
     *
     * @return The current list of control points.
     */
    inline QVector<QPointF> getCurvePoints() const
    {
        return mControlPoints;
    }

private:
    /**
     * @brief Rebuilds the internal lookup table from the current control points.
     *
     * If the LUT is not marked dirty, this function returns immediately.
     * Otherwise, it evaluates MathUtils::evalPressureCurve() at uniformly
     * spaced samples in [0, 1] and stores the results in the LUT.
     */
    inline void rebuildLUT()
    {
        if (!mIsDirty) {
            return;
        }

        for (int i = 0; i < LUT_SIZE; ++i) {
            const qreal x = qreal(i) / qreal(LUT_SIZE - 1);
            mLookupTable[i] = MathUtils::evalPressureCurve(x, mControlPoints);
        }

        mIsDirty = false;
    }

private:
    QVector<QPointF> mControlPoints;              //!< Stored control points for the curve.
    static constexpr int LUT_SIZE = 256;         //!< Size of the lookup table.
    qreal mLookupTable[LUT_SIZE];                //!< Precomputed curve values.
    bool mIsDirty;                               //!< Dirty flag for lazy LUT rebuilding.
};

#endif // PRESSURECURVELUT_H

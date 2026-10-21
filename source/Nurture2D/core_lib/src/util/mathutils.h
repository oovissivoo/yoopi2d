#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <QtMath>
#include <QVector>
#include <QPointF>
#include <algorithm>

namespace MathUtils
{
    /** Get the angle from the difference vector a->b to the x-axis.
     *
     * \param a Start point of vector
     * \param b End point of vector
     * \return Angle in radians from [-pi,+pi]
     */
    inline qreal getDifferenceAngle(const QPointF a, const QPointF b)
    {
        return qAtan2(b.y() - a.y(), b.x() - a.x());
    }

    /** Map one range onto another
     *  \param x The input value
     *  \param inputMin The input min value
     *  \param inputMax The input max value
     *  \param outputMin The output min value
     *  \param outputMax The output max value
     *  \return The value of x mapped to the corresponding range between outputMin and outputMax
     */
    inline qreal map(qreal x, qreal inputMin, qreal inputMax, qreal outputMin, qreal outputMax)
    {
        qreal slope = (outputMax - outputMin) / (inputMax - inputMin);
        return outputMin + slope * (x - inputMin);
    }

    /** Normalize x to a value between 0 and 1;
    *  \param x The input value
    *  \param min The input min value
    *  \param max The input max value
    *  \return The value of x normalized to a range between 0 and 1
    */
    inline qreal normalize(qreal x, qreal min, qreal max)
    {
        return qAbs((x - max) / (min - max));
    }

    inline void blendPixelSourceOver(QRgb src, QRgb& dst)
    {
        const int sa = qAlpha(src);
        if (sa == 0) return;
        if (sa == 255) { dst = src; return; }
        const int invSa = 255 - sa;
        const int dr = qRed(src)   + (qRed(dst)   * invSa) / 255;
        const int dg = qGreen(src) + (qGreen(dst) * invSa) / 255;
        const int db = qBlue(src)  + (qBlue(dst)  * invSa) / 255;
        const int da = sa          + (qAlpha(dst) * invSa) / 255;
        dst = qRgba(dr, dg, db, da);
    }

    inline int clampByte(int value)
    {
        return value < 0 ? 0 : (value > 255 ? 255 : value);
    }

    inline qreal normalizeAngle(qreal angleDeg)
    {
        qreal a = std::fmod(angleDeg, 360.0);
        if (a < 0) a += 360.0;
        return a;
    }

    inline qreal angleDifference(qreal angle1, qreal angle2)
    {
        qreal a1 = normalizeAngle(angle1);
        qreal a2 = normalizeAngle(angle2);
        qreal diff = qAbs(a1 - a2);
        return (diff > 180.0) ? (360.0 - diff) : diff;
    }

    // --- Catmull-Rom (centripetal) spline utilities for pressure curves ---

    inline qreal clampUnit(qreal v) { return qBound<qreal>(0.0, v, 1.0); }

    // Evaluate a single Catmull-Rom segment using Hermite basis with simple tangent estimate
    inline qreal evalCatmullRomSegment(qreal t, qreal p0, qreal p1, qreal p2, qreal p3)
    {
        t = qBound<qreal>(0.0, t, 1.0);
        // Simple centripetal/approximated tangent: (p2 - p0) / 2
        const qreal m1 = 0.5 * (p2 - p0);
        const qreal m2 = 0.5 * (p3 - p1);

        const qreal t2 = t * t;
        const qreal t3 = t2 * t;

        const qreal h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        const qreal h10 = t3 - 2.0 * t2 + t;
        const qreal h01 = -2.0 * t3 + 3.0 * t2;
        const qreal h11 = t3 - t2;

        const qreal v = h00 * p1 + h10 * m1 + h01 * p2 + h11 * m2;
        return clampUnit(v);
    }

    // Ensure control points are sorted by x coordinate
    inline void sortControlPointsByX(QVector<QPointF>& points)
    {
        std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b){ return a.x() < b.x(); });
    }

    // Evaluate multi-point pressure curve (input in [0,1]) using Catmull-Rom interpolation
    // with implicit endpoints (0,0) and (1,1). Control points are sorted by x internally.
    inline qreal evalPressureCurve(qreal inputPressure, const QVector<QPointF>& controlPoints)
    {
        const qreal x = qBound<qreal>(0.0, inputPressure, 1.0);
        if (controlPoints.isEmpty()) {
            return x; // linear fallback
        }

        // Copy and sort control points
        QVector<QPointF> pts = controlPoints;
        sortControlPointsByX(pts);

        // Build extended vector with implicit endpoints, avoiding duplicates near 0 and 1
        const qreal eps = 1e-6;
        QVector<QPointF> ext;
        ext.reserve(pts.size() + 2);
        if (pts.isEmpty() || qAbs(pts.first().x() - 0.0) > eps) {
            ext.append(QPointF(0.0, 0.0));
        }
        ext += pts;
        if (pts.isEmpty() || qAbs(pts.last().x() - 1.0) > eps) {
            ext.append(QPointF(1.0, 1.0));
        }

        const int n = ext.size();
        if (n == 1) {
            // Only an endpoint — linear
            return ext[0].x() <= 0.0 ? ext[0].y() : x;
        }
        if (n == 2) {
            // Linear between the two points (likely endpoints)
            const qreal x0 = ext[0].x(); const qreal y0 = qBound<qreal>(0.0, ext[0].y(), 1.0);
            const qreal x1 = ext[1].x(); const qreal y1 = qBound<qreal>(0.0, ext[1].y(), 1.0);
            const qreal t = (x - x0) / qMax<qreal>(eps, x1 - x0);
            const qreal y = y0 + t * (y1 - y0);
            return clampUnit(y);
        }

        // Find segment where ext[i].x <= x <= ext[i+1].x
        int idx = 0;
        for (int i = 0; i < n - 1; ++i) {
            if (x >= ext[i].x() && x <= ext[i+1].x()) { idx = i; break; }
            if (i == n-2 && x > ext[i+1].x()) idx = i;
        }

        const QPointF& p1 = ext[qBound(0, idx, n-1)];
        const QPointF& p2 = ext[qBound(0, idx+1, n-1)];
        const QPointF p0 = (idx-1 >= 0) ? ext[idx-1] : QPointF(p1.x(), p1.y());
        const QPointF p3 = (idx+2 < n) ? ext[idx+2] : QPointF(p2.x(), p2.y());

        const qreal segMin = p1.x();
        const qreal segMax = p2.x();
        const qreal denom = qMax<qreal>(eps, segMax - segMin);
        const qreal t = (x - segMin) / denom;

        const qreal y = evalCatmullRomSegment(t, p0.y(), p1.y(), p2.y(), p3.y());
        return clampUnit(y);
    }
}

#endif // MATHUTILS_H

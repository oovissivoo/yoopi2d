#include "beziercurveeditor.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLineF>
#include <QtMath>
#include "../CUI/cosmicuimetrics.h"

BezierCurveEditor::BezierCurveEditor(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(200, 140);
    setMouseTracking(true);
    mControlPoints.clear();
    mControlPoints.append(QPointF(0.0, 0.0));
    mControlPoints.append(QPointF(0.5, 0.5));
    mControlPoints.append(QPointF(1.0, 1.0));
    mRemovalZone = QRectF(0.0, 0.9, 0.1, 0.1);
}

void BezierCurveEditor::setControlPoints(const QVector<QPointF>& points)
{
    qreal startY = 0.0, endY = 1.0, midX = 0.5, midY = 0.5;

    if (!points.isEmpty()) {
        const qreal eps = 1e-3;
        bool haveStart = false, haveEnd = false;
        qreal bestMidDist = 1e9;
        for (const QPointF& p : points) {
            const qreal x = qBound(0.0, p.x(), 1.0);
            const qreal y = qBound(0.0, p.y(), 1.0);
            if (!haveStart && qAbs(x - 0.0) < eps) { startY = y; haveStart = true; }
            if (!haveEnd  && qAbs(x - 1.0) < eps) { endY   = y; haveEnd   = true; }
            const qreal d = qAbs(x - 0.5);
            if (d < bestMidDist) { bestMidDist = d; midX = x; midY = y; }
        }
        midX = qBound(0.001, midX, 0.999);
    }

    mControlPoints.clear();
    mControlPoints.append(QPointF(0.0,  qBound(0.0, startY, 1.0)));
    mControlPoints.append(QPointF(midX, qBound(0.0, midY,   1.0)));
    mControlPoints.append(QPointF(1.0,  qBound(0.0, endY,   1.0)));
    update();
}

QVector<QPointF> BezierCurveEditor::getControlPoints() const { return mControlPoints; }
void BezierCurveEditor::addControlPoint(const QPointF& point) { Q_UNUSED(point); }
void BezierCurveEditor::removeControlPoint(int index)         { Q_UNUSED(index);  }

void BezierCurveEditor::resetToDefault()
{
    mControlPoints.clear();
    mControlPoints.append(QPointF(0.0, 0.0));
    mControlPoints.append(QPointF(0.5, 0.5));
    mControlPoints.append(QPointF(1.0, 1.0));
    update();
    emit curveChanged(mControlPoints);
}

QRectF BezierCurveEditor::graphRect() const
{
    const int pad = 16;
    return rect().adjusted(pad, pad, -pad, -pad);
}

QPointF BezierCurveEditor::toScreen(const QPointF& unit) const
{
    QRectF g = graphRect();
    return QPointF(g.left() + unit.x() * g.width(),
                   g.bottom() - unit.y() * g.height());
}

QPointF BezierCurveEditor::toUnit(const QPointF& screen) const
{
    QRectF g = graphRect();
    return QPointF(qBound(0.0, (screen.x() - g.left()) / g.width(),  1.0),
                   qBound(0.0, (g.bottom() - screen.y()) / g.height(), 1.0));
}

void BezierCurveEditor::drawGrid(QPainter& p) const
{
    QRectF g = graphRect();
    p.save();

    // Outer border — slightly lighter than background
    p.setPen(QPen(QColor(0x3a, 0x3a, 0x3a), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(g);

    // 4×4 inner grid lines
    p.setPen(QPen(QColor(0x2e, 0x2e, 0x2e), 1));
    for (int i = 1; i < 4; ++i) {
        const qreal x = g.left() + g.width()  * (i / 4.0);
        const qreal y = g.top()  + g.height() * (i / 4.0);
        p.drawLine(QPointF(x, g.top()),    QPointF(x, g.bottom()));
        p.drawLine(QPointF(g.left(), y),   QPointF(g.right(), y));
    }

    p.restore();
}

void BezierCurveEditor::drawHandles(QPainter& p) const
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false); // crisp squares

    const QColor accent = CosmicColor::accent();

    // Connecting lines between handles
    p.setPen(QPen(accent, 1));
    for (int i = 0; i + 1 < mControlPoints.size(); ++i) {
        p.drawLine(toScreen(mControlPoints[i]), toScreen(mControlPoints[i + 1]));
    }

    // Outline squares for each handle
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < mControlPoints.size(); ++i) {
        const QPointF s = toScreen(mControlPoints[i]);
        qreal half = mHandleRadius;
        if (i == mDraggedPointIndex) half += 2.0;
        else if (i == mHoveredPointIndex) half += 1.0;

        p.setPen(QPen(accent, 1.5));
        p.drawRect(QRectF(s.x() - half, s.y() - half, half * 2, half * 2));
    }

    p.restore();
}

void BezierCurveEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x1f, 0x1f, 0x1f, 0xff));
    drawGrid(p);
    drawHandles(p);
}

void BezierCurveEditor::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::RightButton) return;

    for (int i = 0; i < mControlPoints.size(); ++i) {
        if (QLineF(e->pos(), toScreen(mControlPoints[i])).length() <= mHandleRadius * 2.0) {
            mDragging = DragPoint;
            mDraggedPointIndex = i;
            setCursor(Qt::ClosedHandCursor);
            update();
            return;
        }
    }
    mDragging = None;
}

void BezierCurveEditor::mouseMoveEvent(QMouseEvent* e)
{
    if (mDragging == None) {
        int found = -1;
        for (int i = 0; i < mControlPoints.size(); ++i) {
            if (QLineF(e->pos(), toScreen(mControlPoints[i])).length() <= mHandleRadius * 2.0) {
                found = i; break;
            }
        }
        if (found != mHoveredPointIndex) { mHoveredPointIndex = found; update(); }
        return;
    }

    if (mDragging == DragPoint && mDraggedPointIndex >= 0) {
        QPointF u = toUnit(e->pos());
        setCursor(Qt::ClosedHandCursor);
        if (mDraggedPointIndex == 0) {
            mControlPoints[0].setX(0.0);
            mControlPoints[0].setY(u.y());
        } else if (mDraggedPointIndex == mControlPoints.size() - 1) {
            mControlPoints[mControlPoints.size() - 1].setX(1.0);
            mControlPoints[mControlPoints.size() - 1].setY(u.y());
        } else {
            mControlPoints[mDraggedPointIndex] = QPointF(qBound(0.001, u.x(), 0.999), u.y());
        }
        update();
        emit curveChanged(mControlPoints);
    }
}

void BezierCurveEditor::mouseReleaseEvent(QMouseEvent*)
{
    if (mDraggedPointIndex >= 0) emit curveCommitted(mControlPoints);
    mDragging = None;
    mDraggedPointIndex = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

void BezierCurveEditor::mouseDoubleClickEvent(QMouseEvent* e) { Q_UNUSED(e); }

void BezierCurveEditor::leaveEvent(QEvent*)
{
    mHoveredPointIndex = -1;
    update();
}

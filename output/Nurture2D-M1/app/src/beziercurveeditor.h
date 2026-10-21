#ifndef BEZIERCURVEEDITOR_H
#define BEZIERCURVEEDITOR_H

#include <QWidget>
#include <QPointF>

class BezierCurveEditor : public QWidget
{
    Q_OBJECT
public:
    explicit BezierCurveEditor(QWidget* parent = nullptr);

    void setControlPoints(const QVector<QPointF>& points);
    QVector<QPointF> getControlPoints() const;
    void addControlPoint(const QPointF& point);
    void removeControlPoint(int index);
    void resetToDefault();

signals:
    void curveChanged(const QVector<QPointF>& points);
    void curveCommitted(const QVector<QPointF>& points);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QRectF graphRect() const; // unit square drawing rect inside widget
    QPointF toScreen(const QPointF& unit) const; // unit [0..1] -> screen
    QPointF toUnit(const QPointF& screen) const; // screen -> unit [0..1]

    void drawGrid(QPainter& p) const;
    void drawHandles(QPainter& p) const;

    QVector<QPointF> mControlPoints;
    enum DragTarget { None, DragPoint };
    DragTarget mDragging = None;
    int mDraggedPointIndex = -1;
    int mHoveredPointIndex = -1;
    QRectF mRemovalZone; // in unit coords
    qreal mHandleRadius = 6.0;

    // Context delete UI (screen-space)
    int mContextDeleteIndex = -1;
    QRectF mDeleteButtonRect; // in screen coords
};

#endif // BEZIERCURVEEDITOR_H 
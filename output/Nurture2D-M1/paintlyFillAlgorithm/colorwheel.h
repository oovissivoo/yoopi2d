#ifndef COLORWHEEL_H
#define COLORWHEEL_H

#include <QWidget>
#include <QColor>
#include <QPainter>
#include <QMouseEvent>

class ColorWheel : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheel(QWidget *parent = nullptr);
    QColor color() const;
    void setColor(const QColor &color);

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void drawWheel(QPainter &painter);
    void drawTriangle(QPainter &painter);
    void drawIndicators(QPainter &painter);
    void updateColorFromWheelPos(const QPoint &pos);
    void updateColorFromTrianglePos(const QPoint &pos);
    bool isInWheel(const QPoint &pos) const;
    bool isInTriangle(const QPoint &pos) const;
    QPointF getTrianglePoint(int index) const;
    QColor colorFromWheelPos(const QPoint &pos) const;
    QColor colorFromTrianglePos(const QPoint &pos) const;
    QPoint wheelPosFromColor(const QColor &color) const;
    QPointF trianglePosFromColor(const QColor &color) const;
    
    QColor m_color;
    QImage m_wheelImage;
    QImage m_triangleImage;
    bool m_draggingWheel;
    bool m_draggingTriangle;
    int m_wheelSize;
    QPoint m_center;
    int m_radius;
    int m_triangleSize;
    qreal m_hue;
};

#endif // COLORWHEEL_H 
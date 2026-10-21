#ifndef COLORWHEEL_H
#define COLORWHEEL_H

#include <QWidget>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>

class ColorWheel : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheel(QWidget *parent = nullptr);
    
    QColor color() const;
    QColor previousColor() const;
    void setColor(const QColor &color);
    void swapColors();

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
    void drawColorPreview(QPainter &painter);
    
    QPointF getTrianglePoint(int index) const;
    
    bool isInWheel(const QPoint &pos) const;
    bool isInTriangle(const QPoint &pos) const;
    bool isInCurrentColorBox(const QPoint &pos) const;
    bool isInPreviousColorBox(const QPoint &pos) const;
    QColor getContrastingColor(const QColor &color) const;
    
    void updateColorFromWheelPos(const QPoint &pos);
    void updateColorFromTrianglePos(const QPoint &pos);
    
    QColor colorFromWheelPos(const QPoint &pos) const;
    QColor colorFromTrianglePos(const QPoint &pos) const;
    
    QPoint wheelPosFromColor(const QColor &color) const;
    QPointF trianglePosFromColor(const QColor &color) const;
    
    QColor m_color;
    QColor m_previousColor;
    QColor m_draggingStartColor;
    qreal m_hue;
    
    QPointF m_center;
    qreal m_radius;
    qreal m_triangleSize;
    qreal m_wheelSize;
    
    bool m_draggingWheel;
    bool m_draggingTriangle;
    
    QImage m_wheelImage;
    QImage m_triangleImage;
    
    QRect m_currentColorRect;
    QRect m_previousColorRect;
};

#endif // COLORWHEEL_H 
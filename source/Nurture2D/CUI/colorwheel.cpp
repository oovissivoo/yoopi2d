#include "colorwheel.h"
#include <QPainterPath>
#include <cmath>

ColorWheel::ColorWheel(QWidget *parent)
    : QWidget(parent)
    , m_draggingWheel(false)
    , m_draggingTriangle(false)
    , m_wheelSize(0)
    , m_hue(0)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_color = Qt::red;
}

QColor ColorWheel::color() const
{
    return m_color;
}

void ColorWheel::setColor(const QColor &color)
{
    if (m_color != color) {
        m_color = color;
        m_hue = color.hueF();
        m_triangleImage = QImage(); // Force triangle redraw when the color changes
        update();
        emit colorChanged(color);
    }
}

void ColorWheel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate dimensions
    m_center = rect().center();
    m_radius = qMin(width(), height()) / 2 - 10;
    m_triangleSize = m_radius * 0.8; // Triangle size relative to wheel
    
    // Draw components
    drawWheel(painter);
    drawTriangle(painter);
    drawIndicators(painter);
}

void ColorWheel::drawWheel(QPainter &painter)
{
    if (m_wheelImage.size() != size()) {
        m_wheelImage = QImage(size(), QImage::Format_ARGB32_Premultiplied);
        m_wheelImage.fill(Qt::transparent);
        
        QPainter imgPainter(&m_wheelImage);
        imgPainter.setRenderHint(QPainter::Antialiasing, true);
        imgPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        
        // Start at 3 o'clock position (0 degrees) with red at right
        QConicalGradient conicalGradient(m_center, 0);
        
        // HSV color wheel: red at 0°, yellow at 60°, green at 120°, cyan at 180°, blue at 240°, magenta at 300°
        // Horizontal flip by using (1.0 - hue) to mirror across the vertical axis
        for (int i = 0; i < 360; i += 1) {
            qreal hue = i / 360.0;
            // Horizontally flip the hue (1.0 - hue mirrors across the vertical axis)
            qreal flippedHue = 1.0 - hue;
            // QConicalGradient goes counter-clockwise from 3 o'clock position
            conicalGradient.setColorAt(hue, QColor::fromHsvF(flippedHue, 1, 1));
        }
        
        // Create the wheel path (an annular ring)
        QPainterPath wheelPath;
        wheelPath.addEllipse(m_center, m_radius, m_radius);
        QPainterPath innerCircle;
        innerCircle.addEllipse(m_center, m_radius - 20, m_radius - 20);
        wheelPath = wheelPath.subtracted(innerCircle);
        
        // Fill the wheel with the conical gradient
        imgPainter.setPen(Qt::NoPen);
        imgPainter.setBrush(conicalGradient);
        imgPainter.drawPath(wheelPath);
    }
    painter.drawImage(0, 0, m_wheelImage);
}

void ColorWheel::drawTriangle(QPainter &painter)
{
    // Always regenerate triangle when the size, null state, or hue changes
    static qreal lastHue = -1;  // Track last used hue
    if (m_triangleImage.size() != size() || m_triangleImage.isNull() || lastHue != m_hue) {
        lastHue = m_hue;  // Update the last used hue
        m_triangleImage = QImage(size(), QImage::Format_ARGB32_Premultiplied);
        m_triangleImage.fill(Qt::transparent);
        
        QPainter imgPainter(&m_triangleImage);
        imgPainter.setRenderHint(QPainter::Antialiasing);
        
        QPointF points[3] = {
            getTrianglePoint(0),
            getTrianglePoint(1),
            getTrianglePoint(2)
        };
        
        // Create the triangle path
        QPainterPath trianglePath;
        trianglePath.moveTo(points[0]);
        trianglePath.lineTo(points[1]);
        trianglePath.lineTo(points[2]);
        trianglePath.lineTo(points[0]);

        // Create pure hue color based on current hue
        QColor pureHueColor = QColor::fromHsvF(m_hue, 1.0, 1.0);

        // Create a gradient for the saturation (white to pure hue color)
        QLinearGradient gradientSaturation(points[0], points[1]);
        gradientSaturation.setColorAt(0, Qt::white);
        gradientSaturation.setColorAt(1, pureHueColor);
        
        // Fill with the saturation gradient
        imgPainter.fillPath(trianglePath, gradientSaturation);
        
        // Apply the value (brightness) gradient from the filled triangle to black
        QLinearGradient gradientValue(points[0], points[2]);
        gradientValue.setColorAt(0, QColor(0, 0, 0, 0));  // Transparent
        gradientValue.setColorAt(1, Qt::black);
        
        // Overlay the value gradient
        imgPainter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        imgPainter.fillPath(trianglePath, gradientValue);
    }
    painter.drawImage(0, 0, m_triangleImage);
}

void ColorWheel::drawIndicators(QPainter &painter)
{
    // Draw wheel selector
    QPoint wheelPos = wheelPosFromColor(QColor::fromHsvF(m_hue, 1.0, 1.0));
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(wheelPos, 4, 4);
    
    // Draw triangle selector
    QPointF trianglePos = trianglePosFromColor(m_color);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(trianglePos, 4, 4);
}

QPointF ColorWheel::getTrianglePoint(int index) const
{
    // Calculate the inner wheel radius (where the colored wheel actually is)
    qreal innerWheelRadius = m_radius - 20; // Accounting for wheel thickness
    
    switch (index) {
        case 0: // White point (top)
            return QPointF(m_center.x() - innerWheelRadius * 0.35, m_center.y() - innerWheelRadius * 0.85);
        case 1: // Pure color point (right)
            return QPointF(m_center.x() + innerWheelRadius * 0.95, m_center.y());
        case 2: // Black point (bottom)
            return QPointF(m_center.x() - innerWheelRadius * 0.35, m_center.y() + innerWheelRadius * 0.85);
        default:
            return QPointF();
    }
}

void ColorWheel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        if (isInWheel(pos)) {
            m_draggingWheel = true;
            updateColorFromWheelPos(pos);
        } else if (isInTriangle(pos)) {
            m_draggingTriangle = true;
            updateColorFromTrianglePos(pos);
        }
    }
}

void ColorWheel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingWheel) {
        updateColorFromWheelPos(event->pos());
    } else if (m_draggingTriangle) {
        updateColorFromTrianglePos(event->pos());
    }
}

void ColorWheel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_draggingWheel = false;
        m_draggingTriangle = false;
    }
}

void ColorWheel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_wheelImage = QImage();
    m_triangleImage = QImage();
    update();
}

bool ColorWheel::isInWheel(const QPoint &pos) const
{
    QPointF relPos = pos - m_center;
    qreal dist = sqrt(relPos.x() * relPos.x() + relPos.y() * relPos.y());
    return dist <= m_radius && dist >= m_radius - 20;
}

bool ColorWheel::isInTriangle(const QPoint &pos) const
{
    QPointF points[3] = {
        getTrianglePoint(0),
        getTrianglePoint(1),
        getTrianglePoint(2)
    };
    
    QPointF v0 = points[2] - points[0];
    QPointF v1 = points[1] - points[0];
    QPointF v2 = pos - points[0];
    
    qreal dot00 = QPointF::dotProduct(v0, v0);
    qreal dot01 = QPointF::dotProduct(v0, v1);
    qreal dot02 = QPointF::dotProduct(v0, v2);
    qreal dot11 = QPointF::dotProduct(v1, v1);
    qreal dot12 = QPointF::dotProduct(v1, v2);
    
    qreal invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    qreal u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    qreal v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    
    return (u >= 0) && (v >= 0) && (u + v <= 1);
}

void ColorWheel::updateColorFromWheelPos(const QPoint &pos)
{
    QPointF relPos = pos - m_center;
    
    // Calculate the angle in radians, atan2 gives angle from the positive x-axis
    qreal angle = atan2(relPos.y(), relPos.x());
    
    // Convert to positive angle in [0, 2π] range
    if (angle < 0) angle += 2 * M_PI;
    
    // Convert angle to hue value between 0 and 1
    // Since atan2 goes counter-clockwise and so does QConicalGradient,
    // we can directly map the angle to hue without flipping
    qreal newHue = angle / (2 * M_PI);
    
    // Ensure hue is in [0, 1] range
    if (newHue >= 1.0) newHue -= 1.0;
    if (newHue < 0.0) newHue += 1.0;
    
    // Only invalidate triangle image and trigger redraw if hue actually changed
    // Use a small epsilon to avoid unnecessary updates for tiny changes
    if (qAbs(m_hue - newHue) > 1e-6) {
        m_hue = newHue;
        m_triangleImage = QImage(); // Force triangle redraw
    }
    
    // Update color with the new hue while preserving saturation and value
    m_color.setHsvF(m_hue, m_color.saturationF(), m_color.valueF());
    
    update();
    emit colorChanged(m_color);
}

void ColorWheel::updateColorFromTrianglePos(const QPoint &pos)
{
    QPointF points[3] = {
        getTrianglePoint(0),  // White (top)
        getTrianglePoint(1),  // Pure color (right)
        getTrianglePoint(2)   // Black (bottom)
    };
    
    // Convert mouse position to barycentric coordinates
    QPointF v0 = points[2] - points[0];
    QPointF v1 = points[1] - points[0];
    QPointF v2 = pos - points[0];
    
    qreal dot00 = QPointF::dotProduct(v0, v0);
    qreal dot01 = QPointF::dotProduct(v0, v1);
    qreal dot02 = QPointF::dotProduct(v0, v2);
    qreal dot11 = QPointF::dotProduct(v1, v1);
    qreal dot12 = QPointF::dotProduct(v1, v2);
    
    qreal invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    qreal u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    qreal v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    
    // Convert barycentric coordinates to HSV
    qreal value = 1.0 - u;
    qreal saturation = v / (1.0 - u);
    if (std::isnan(saturation)) saturation = 0;
    
    saturation = qBound(0.0, saturation, 1.0);
    value = qBound(0.0, value, 1.0);
    
    m_color.setHsvF(m_hue, saturation, value);
    update();
    emit colorChanged(m_color);
}

QColor ColorWheel::colorFromWheelPos(const QPoint &pos) const
{
    QPointF relPos = pos - m_center;
    
    // Calculate the angle in radians
    qreal angle = atan2(relPos.y(), relPos.x());
    
    // Convert to positive angle in [0, 2π] range
    if (angle < 0) angle += 2 * M_PI;
    
    // Convert angle to hue value between 0 and 1
    qreal hue = angle / (2 * M_PI);
    
    // Ensure hue is in [0, 1] range
    if (hue >= 1.0) hue -= 1.0;
    if (hue < 0.0) hue += 1.0;
    
    return QColor::fromHsvF(hue, 1, 1);
}

QColor ColorWheel::colorFromTrianglePos(const QPoint &pos) const
{
    QPointF points[3] = {
        getTrianglePoint(0),  // White (top)
        getTrianglePoint(1),  // Pure color (right)
        getTrianglePoint(2)   // Black (bottom)
    };
    
    QPointF v0 = points[2] - points[0];
    QPointF v1 = points[1] - points[0];
    QPointF v2 = pos - points[0];
    
    qreal dot00 = QPointF::dotProduct(v0, v0);
    qreal dot01 = QPointF::dotProduct(v0, v1);
    qreal dot02 = QPointF::dotProduct(v0, v2);
    qreal dot11 = QPointF::dotProduct(v1, v1);
    qreal dot12 = QPointF::dotProduct(v1, v2);
    
    qreal invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    qreal u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    qreal v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    
    qreal value = 1.0 - u;
    qreal saturation = v / (1.0 - u);
    if (std::isnan(saturation)) saturation = 0;
    
    return QColor::fromHsvF(m_hue, qBound(0.0, saturation, 1.0), qBound(0.0, value, 1.0));
}

QPoint ColorWheel::wheelPosFromColor(const QColor &color) const
{
    // Get the hue and convert to angle in radians
    qreal angle = color.hueF() * 2 * M_PI;
    
    // Ensure angle is in [0, 2π] range
    if (angle < 0) angle += 2 * M_PI;
    if (angle >= 2 * M_PI) angle -= 2 * M_PI;
    
    // Convert polar coordinates to Cartesian for the wheel position
    qreal x = m_center.x() + (m_radius - 10) * cos(angle);
    qreal y = m_center.y() + (m_radius - 10) * sin(angle);
    
    return QPoint(x, y);
}

QPointF ColorWheel::trianglePosFromColor(const QColor &color) const
{
    QPointF points[3] = {
        getTrianglePoint(0),  // White (top)
        getTrianglePoint(1),  // Pure color (right)
        getTrianglePoint(2)   // Black (bottom)
    };
    
    qreal s = color.saturationF();
    qreal v = color.valueF();
    
    // Convert HSV coordinates to barycentric coordinates
    qreal b = 1.0 - v;        // Black component (bottom)
    qreal c = s * v;          // Pure color component (right)
    qreal w = 1.0 - b - c;    // White component (top)
    
    return points[0] * w + points[1] * c + points[2] * b;
} 
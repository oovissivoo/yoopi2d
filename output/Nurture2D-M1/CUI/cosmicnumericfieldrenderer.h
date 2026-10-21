#ifndef COSMICNUMERICFIELDRENDERER_H
#define COSMICNUMERICFIELDRENDERER_H

#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QRect>

namespace CosmicNumericFieldRenderer {

struct PaintParams {
    QRect widgetRect;
    QRect leftRect;
    QRect centerRect;
    QRect rightRect;

    QColor accentColor;
    QColor borderColor;
    int borderWidth = 1;
    int cornerRadius = 0;

    bool legacySidePanels = false;

    int hoverRegion = 0;   // 0 None, 1 Left, 2 Center, 3 Right
    int pressedRegion = 0; // 0 None, 1 Left, 2 Center, 3 Right
    bool dragging = false;

    bool antialias = false;

    bool drawArrows = true;
    QSize arrowSize = QSize(18, 18);
    qreal arrowOpacityNormal = 0.7;
    qreal arrowOpacityHighlighted = 1.0;
    bool tintArrows = true;
    QColor arrowTint;
};

void paintField(QPainter& painter, const PaintParams& p);

} // namespace CosmicNumericFieldRenderer

#endif // COSMICNUMERICFIELDRENDERER_H

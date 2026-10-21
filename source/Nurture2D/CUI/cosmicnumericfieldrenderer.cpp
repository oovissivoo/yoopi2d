#include "cosmicnumericfieldrenderer.h"

#include <QPainterPath>
#include <QtMath>

namespace {
static QPixmap loadArrowPixmap(const QString& path, const QSize& size)
{
    QPixmap pix(path);
    if (pix.isNull())
        return QPixmap();
    return pix.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

static QPixmap tintPixmap(const QPixmap& src, const QColor& tint)
{
    if (src.isNull() || !tint.isValid())
        return src;

    QPixmap tinted(src.size());
    tinted.fill(Qt::transparent);
    {
        QPainter p(&tinted);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.drawPixmap(0, 0, src);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(tinted.rect(), tint);
    }
    return tinted;
}
}

namespace CosmicNumericFieldRenderer {

void paintField(QPainter& painter, const PaintParams& p)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, p.antialias && p.cornerRadius > 0);

    const QColor centerBase = p.accentColor;
    const QColor sideBase = p.legacySidePanels ? QColor(31, 31, 31) : centerBase;

    QColor leftBg = sideBase;
    if (p.pressedRegion == 1) {
        leftBg = sideBase.lighter(130);
    } else if (p.hoverRegion == 1) {
        leftBg = sideBase.lighter(115);
    }

    QColor rightBg = sideBase;
    if (p.pressedRegion == 3) {
        rightBg = sideBase.lighter(130);
    } else if (p.hoverRegion == 3) {
        rightBg = sideBase.lighter(115);
    }

    QColor centerBg = centerBase;
    if (p.dragging) {
        centerBg = centerBase.darker(115);
    } else if (p.pressedRegion == 2) {
        centerBg = centerBase.darker(125);
    } else if (p.hoverRegion == 2) {
        centerBg = centerBase.lighter(110);
    }

    // Flat, borderless squircle look (the desired modern style) is the default.
    // For legacy side panels, we still allow optional border drawing.
    const int bw = (p.legacySidePanels ? qMax(0, p.borderWidth) : 0);

    // Use a rounded clip path so inner segment fills can never leave sharp remnants
    // at the corners, regardless of rect math.
    QRectF outerF(p.widgetRect);
    if (p.antialias && p.cornerRadius > 0) {
        outerF = outerF.adjusted(0.5, 0.5, -0.5, -0.5);
    }

    QPainterPath outerPath;
    if (p.cornerRadius > 0) {
        outerPath.addRoundedRect(outerF, p.cornerRadius, p.cornerRadius);
    } else {
        outerPath.addRect(outerF);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(sideBase);
    painter.drawPath(outerPath);
    painter.setClipPath(outerPath);

    QRect leftInner = p.leftRect;
    QRect centerInner = p.centerRect;
    QRect rightInner = p.rightRect;
    if (bw > 0) {
        leftInner = leftInner.adjusted(bw, bw, 0, -bw);
        centerInner = centerInner.adjusted(0, bw, 0, -bw);
        rightInner = rightInner.adjusted(0, bw, -bw, -bw);
    }

    painter.fillRect(leftInner, leftBg);
    painter.fillRect(centerInner, centerBg);
    painter.fillRect(rightInner, rightBg);

    if (bw > 0 && p.borderColor.isValid()) {
        painter.setClipping(false);
        QRectF borderRect = outerF.adjusted(bw / 2.0, bw / 2.0, -bw / 2.0, -bw / 2.0);
        painter.setPen(QPen(p.borderColor, bw));
        painter.setBrush(Qt::NoBrush);
        if (p.cornerRadius > 0) {
            painter.drawRoundedRect(borderRect, p.cornerRadius, p.cornerRadius);
        } else {
            painter.drawRect(borderRect);
        }
    }

    if (p.drawArrows) {
        static const QString leftPath = QStringLiteral(":/Interaction/ArrowLeft.svg");
        static const QString rightPath = QStringLiteral(":/Interaction/ArrowRight.svg");

        const QPixmap leftRaw = loadArrowPixmap(leftPath, p.arrowSize);
        const QPixmap rightRaw = loadArrowPixmap(rightPath, p.arrowSize);

        const QColor tint = (p.tintArrows && p.arrowTint.isValid()) ? p.arrowTint : QColor();
        const QPixmap leftPix = tint.isValid() ? tintPixmap(leftRaw, tint) : leftRaw;
        const QPixmap rightPix = tint.isValid() ? tintPixmap(rightRaw, tint) : rightRaw;

        const bool anyHoverOrDrag = (p.hoverRegion != 0) || p.dragging;
        const bool leftHighlight = (p.hoverRegion == 1 || p.pressedRegion == 1);
        const bool rightHighlight = (p.hoverRegion == 3 || p.pressedRegion == 3);

        auto drawArrow = [&painter](const QRect& rect,
                                   const QPixmap& pix,
                                   bool visible,
                                   bool highlighted,
                                   qreal opNormal,
                                   qreal opHighlight) {
            if (!visible || pix.isNull())
                return;
            painter.save();
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.setOpacity(highlighted ? opHighlight : opNormal);

            QPoint topLeft(rect.center().x() - pix.width() / 2,
                           rect.center().y() - pix.height() / 2);
            painter.drawPixmap(topLeft, pix);
            painter.restore();
        };

        drawArrow(p.leftRect, leftPix, anyHoverOrDrag, leftHighlight, p.arrowOpacityNormal, p.arrowOpacityHighlighted);
        drawArrow(p.rightRect, rightPix, anyHoverOrDrag, rightHighlight, p.arrowOpacityNormal, p.arrowOpacityHighlighted);
    }

    painter.restore();
}

} // namespace CosmicNumericFieldRenderer

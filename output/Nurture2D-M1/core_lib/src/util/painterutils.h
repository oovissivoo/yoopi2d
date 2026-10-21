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

#ifndef PAINTERUTILS_H
#define PAINTERUTILS_H

#include <QtMath>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QVector>

#include "../../../CUI/cosmicuimetrics.h"

/** Calculate layer opacity based on current layer offset */
inline qreal calculateRelativeOpacityForLayer(int currentLayerIndex, int layerIndexNext, float threshold)
{
    int layerOffset = currentLayerIndex - layerIndexNext;
    int absoluteOffset = qAbs(layerOffset);
    qreal newOpacity = 1.0;
    if (absoluteOffset != 0)
    {
        newOpacity = qPow(static_cast<qreal>(threshold), absoluteOffset);
    }
    return newOpacity;
};

// ─── Tool overlay shared drawing rules ───────────────────────────────────────
//
// All curve-family tools (CurveTool, BSplineTool, PolylineTool) must use these
// helpers so that visual style changes only need to happen in one place.
//
// Rule: connect points / draw previews with dashes colored from CosmicAccentColor.

namespace ToolOverlay {

static const QVector<qreal> kDashPattern = { 6.0, 6.0 };
static const qreal kDashWidth  = 1.5;
static const qreal kDotRadius  = 4.0;
static const QColor kOutline   = QColor(60, 60, 60);

/// Draw a single accent-colored dashed line between two points (canvas space).
/// Pen is cosmetic so width stays constant at all zoom levels.
inline void drawHandleLine(QPainter& painter, const QPointF& a, const QPointF& b)
{
    const QColor accent = CosmicColor::accent();
    QPen pen(accent, kDashWidth, Qt::CustomDashLine);
    pen.setDashPattern(kDashPattern);
    pen.setCapStyle(Qt::FlatCap);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(a, b);
}

/// Draw accent-colored dashed lines connecting a list of screen-space points in order.
/// Pass closeLoop=true to also connect last→first.
inline void drawHandleChain(QPainter& painter, const QVector<QPointF>& screenPts, bool closeLoop = false)
{
    if (screenPts.size() < 2) return;
    const QColor accent = CosmicColor::accent();
    QPen pen(accent, kDashWidth, Qt::CustomDashLine);
    pen.setDashPattern(kDashPattern);
    pen.setCapStyle(Qt::FlatCap);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i + 1 < screenPts.size(); ++i)
        painter.drawLine(screenPts[i], screenPts[i + 1]);
    if (closeLoop)
        painter.drawLine(screenPts.last(), screenPts.first());
}

/// Draw a preview path: dark grey outline pass + accent pass (solid).
/// Pens are cosmetic so width stays constant at all zoom levels.
inline void drawPreviewPath(QPainter& painter, const QPainterPath& path)
{
    const QColor accent = CosmicColor::accent();
    painter.setBrush(Qt::NoBrush);

    QPen outlinePen(kOutline, 3.5, Qt::SolidLine);
    outlinePen.setCapStyle(Qt::RoundCap);
    outlinePen.setJoinStyle(Qt::RoundJoin);
    outlinePen.setCosmetic(true);
    painter.setPen(outlinePen);
    painter.drawPath(path);

    QPen accentPen(accent, kDashWidth, Qt::SolidLine);
    accentPen.setCapStyle(Qt::RoundCap);
    accentPen.setJoinStyle(Qt::RoundJoin);
    accentPen.setCosmetic(true);
    painter.setPen(accentPen);
    painter.drawPath(path);
}

/// Draw a control point dot (accent fill, dark grey outline). Canvas space.
/// Uses cosmetic pen so outline width is zoom-independent.
inline void drawControlDot(QPainter& painter, const QPointF& pos, qreal radius = kDotRadius)
{
    const QColor accent = CosmicColor::accent();
    QPen pen(kOutline, 1.5);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(accent);
    painter.drawEllipse(pos, radius, radius);
}

/// Draw a control point dot in screen space (cosmetic pen).
inline void drawControlDotScreen(QPainter& painter, const QPointF& screenPos, qreal radius = kDotRadius)
{
    const QColor accent = CosmicColor::accent();
    QPen pen(kOutline, 1.5);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(accent);
    painter.drawEllipse(QRectF(screenPos.x() - radius, screenPos.y() - radius, radius * 2, radius * 2));
}

} // namespace ToolOverlay

#endif // PAINTERUTILS_H

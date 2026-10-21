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
#include "selectionpainter.h"

#include "object.h"
#include "qpainter.h"
#include "basetool.h"
#include <cmath>
#include "../../CUI/cosmicuimetrics.h"

SelectionPainter::SelectionPainter()
{
}

void SelectionPainter::paint(QPainter& painter,
                             const Object* object,
                             int layerIndex,
                             BaseTool* tool,
                             TransformParameters& tParams)
{
    Layer* layer = object->getLayer(layerIndex);

    if (layer == nullptr) { return; }

    QTransform transform = tParams.selectionTransform * tParams.viewTransform;
    QPolygonF projectedSelectionPolygon = transform.map(tParams.originalSelectionRectF);

    if (!tParams.selectionPath.isEmpty())
    {
        QPainterPath toDraw;
        if (tParams.drawBoundingBoxForPath) {
            QRectF rect = tParams.selectionPath.boundingRect();
            QPainterPath box; box.addRect(rect);
            toDraw = box;
        } else {
            // Draw the actual lasso path while creating
            toDraw = tParams.selectionPath;
        }
        QPainterPath projectedPath = transform.map(toDraw);
        painter.setBrush(Qt::NoBrush);

        QVector<qreal> dash = { 4.0, 4.0 };
        QPen blackPen(Qt::black, 1.0, Qt::DashLine);
        blackPen.setDashPattern(dash);
        blackPen.setDashOffset(mAntsPhase * 0.5);
        QPen whitePen(Qt::white, 1.0, Qt::DashLine);
        whitePen.setDashPattern(dash);
        whitePen.setDashOffset(mAntsPhase * 0.5 + 4.0);

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(blackPen);
        painter.drawPath(projectedPath);
        painter.setPen(whitePen);
        painter.drawPath(projectedPath);

        mAntsPhase = (mAntsPhase + 1) % 1000;

        // Draw corner handles and rotation ring only in bounding-box mode
        if (tParams.drawBoundingBoxForPath) {
            QRectF rect = tParams.selectionPath.boundingRect();
            QPolygonF projectedRect = transform.map(QPolygonF(rect));

            const QColor accentColor = CosmicColor::accent();
            QPen linePen(accentColor, 1.5);
            linePen.setCosmetic(true);
            QPen handlePen(accentColor, 1.5);
            handlePen.setCosmetic(true);

            const QPointF tl = projectedRect[0];
            const QPointF tr = projectedRect[1];
            const QPointF br = projectedRect[2];
            const QPointF bl = projectedRect[3];
            const QPointF moveCenter = (tl + tr + br + bl) * 0.25;

            // ── Bounding box edges ──────────────────────────────────────────
            painter.setPen(linePen);
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(tl, tr);
            painter.drawLine(tr, br);
            painter.drawLine(br, bl);
            painter.drawLine(bl, tl);

            // ── Corner scale handles: outline squares, no fill ──────────────
            {
                constexpr qreal half = 6.0;
                painter.setPen(handlePen);
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(QRectF(tl.x() - half, tl.y() - half, half * 2, half * 2));
                painter.drawRect(QRectF(tr.x() - half, tr.y() - half, half * 2, half * 2));
                painter.drawRect(QRectF(br.x() - half, br.y() - half, half * 2, half * 2));
                painter.drawRect(QRectF(bl.x() - half, bl.y() - half, half * 2, half * 2));
            }

            // ── Move handle: outline square at bbox center ──────────────────
            {
                constexpr qreal half = 8.0;
                painter.setPen(handlePen);
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(QRectF(moveCenter.x() - half, moveCenter.y() - half, half * 2, half * 2));
            }

            // ── Rotation handle: outline square at midpoint of right edge ───
            {
                constexpr qreal half = 6.0;
                const QPointF rotCenter = (tr + br) * 0.5;
                painter.setPen(linePen);
                painter.drawLine(moveCenter, rotCenter);
                painter.setPen(handlePen);
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(QRectF(rotCenter.x() - half, rotCenter.y() - half, half * 2, half * 2));
            }
        }
    }
    else
    {
        if (layer->type() == Layer::BITMAP)
        {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(Qt::DashLine));
            painter.drawPolygon(projectedSelectionPolygon.toPolygon());
        }
        if (layer->type() == Layer::VECTOR)
        {
            painter.setBrush(QColor(0, 0, 0, 20));
            painter.setPen(Qt::gray);
            painter.drawPolygon(projectedSelectionPolygon);
        }
    }

    if ((tool->type() == SELECT || tool->type() == MOVE) && tParams.selectionPath.isEmpty())
    {
        painter.setPen(Qt::SolidLine);
        painter.setBrush(QBrush(Qt::gray));
        int radius = HANDLE_WIDTH / 2;

        const QRectF topLeftCorner = QRectF(projectedSelectionPolygon[0].x() - radius,
                                            projectedSelectionPolygon[0].y() - radius,
                                            HANDLE_WIDTH, HANDLE_WIDTH);
        painter.drawRect(topLeftCorner);

        const QRectF topRightCorner = QRectF(projectedSelectionPolygon[1].x() - radius,
                                             projectedSelectionPolygon[1].y() - radius,
                                             HANDLE_WIDTH, HANDLE_WIDTH);
        painter.drawRect(topRightCorner);

        const QRectF bottomRightCorner = QRectF(projectedSelectionPolygon[2].x() - radius,
                                                projectedSelectionPolygon[2].y() - radius,
                                                HANDLE_WIDTH, HANDLE_WIDTH);
        painter.drawRect(bottomRightCorner);

        const QRectF bottomLeftCorner = QRectF(projectedSelectionPolygon[3].x() - radius,
                                               projectedSelectionPolygon[3].y() - radius,
                                               HANDLE_WIDTH, HANDLE_WIDTH);
        painter.drawRect(bottomLeftCorner);

        // Keep corners only for rect-based selection
    }

    // Suppress size/diff text for lasso tool to keep gizmo clean
    if (tool->type() != LASSO_SELECT && tool->properties.showSelectionInfo) {
        paintSelectionInfo(painter, transform, tParams.viewTransform, tParams.originalSelectionRectF, projectedSelectionPolygon);
    }
}

void SelectionPainter::paintSelectionInfo(QPainter& painter, const QTransform& mergedTransform, const QTransform& viewTransform, const QRectF& selectionRect, const QPolygonF& projectedPolygonF)
{
    QRect projectedSelectionRect = mergedTransform.mapRect(selectionRect).toAlignedRect();
    QRect originalSelectionRect = viewTransform.mapRect(selectionRect).toAlignedRect();
    QPolygon projectedPolygon = projectedPolygonF.toPolygon();

    QPoint projectedCenter = projectedSelectionRect.center();
    QPoint originalCenter = originalSelectionRect.center();
    int diffX = static_cast<int>(projectedCenter.x() - originalCenter.x());
    int diffY = static_cast<int>(originalCenter.y() - projectedCenter.y());
    painter.drawText(projectedPolygon[0] - QPoint(HANDLE_WIDTH, HANDLE_WIDTH),
                    QString("Size: %1x%2. Diff: %3, %4.").arg(QString::number(selectionRect.width()),
                                                      QString::number(selectionRect.height()),
                                                      QString::number(diffX),
                                                      QString::number(diffY)));
}

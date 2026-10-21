/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef CURVETOOL_H
#define CURVETOOL_H

#include "basetool.h"

#include <QPointF>
#include <QPainterPath>

class QPen;

class CurveTool : public BaseTool
{
    Q_OBJECT
public:
    explicit CurveTool(QObject* parent = nullptr);

    ToolType type() override;
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;
    void resetToDefault() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    bool keyPressEvent(QKeyEvent* event) override;

    void clearToolData() override;

    bool leavingThisTool() override;
    bool isActive() const override;

    // Ensure width changes are marked dirty for persistence on tool switch
    void setWidth(qreal w) override;

private:
    enum class Phase { Idle, DragLine, AdjustCurve };

    Phase mPhase = Phase::Idle;
    QPointF mStartPoint;
    QPointF mEndPoint;
    QPointF mControlPoint;
    QPainterPath mPreviewPath; // Vector preview path
    bool mIsDragging = false;  // Active drag state - use lightweight preview
    QPointF mLivePoint;        // Current cursor position during DragLine

    void drawTemporaryLineTo(const QPointF& endPoint);
    void drawTemporaryCurve(const QPointF& controlPoint);
    void commitCurve();
    void paintOverlay(QPainter& painter) override;
};

#endif // CURVETOOL_H 
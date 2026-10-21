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

#ifndef ENHANCEDVECTORSTROKE_H
#define ENHANCEDVECTORSTROKE_H

#include "beziercurve.h"
#include <QString>
#include <QPointF>

// Enhanced vector stroke that stores brush metadata for re-rendering
class EnhancedVectorStroke : public BezierCurve
{
public:
    explicit EnhancedVectorStroke();
    explicit EnhancedVectorStroke(const QList<QPointF>& pointList, const QList<qreal>& pressureList, double tol, bool smooth = true);
    
    // Brush metadata
    struct BrushMetadata {
        // Tool type and basic properties
        int toolType = 0; // PENCIL, PEN, BRUSH, etc.
        qreal baseWidth = 1.0;
        qreal baseFeather = 0.0;
        bool usePressure = true;
        bool useFeather = false;
        bool useAA = false;
        
        // Pencil-specific texture settings
        int texturePreset = 0;
        qreal textureSpacing = 0.25;
        qreal textureScatter = 0.05;
        qreal textureRotation = 0.0;
        qreal textureScale = 1.0;
        qreal textureFlow = 1.0;
        QString texturePath;
        
        // Pressure curves (for pen/pencil)
        QPointF pressureCurveP1 = QPointF(0.25, 0.0);
        QPointF pressureCurveP2 = QPointF(0.75, 1.0);
        
        // Size/opacity curves (for pencil)
        QPointF sizeCurveP1 = QPointF(0.25, 0.0);
        QPointF sizeCurveP2 = QPointF(0.75, 1.0);
        QPointF opacityCurveP1 = QPointF(0.25, 0.0);
        QPointF opacityCurveP2 = QPointF(0.75, 1.0);
        
        // Stroke timing data for re-stamping
        QList<qreal> dabDistances; // cumulative distances for each dab
        QList<qreal> dabSizes;     // size at each dab point
        QList<qreal> dabOpacities; // opacity at each dab point
    };
    
    // Accessors
    const BrushMetadata& getBrushMetadata() const { return mBrushMetadata; }
    BrushMetadata& getBrushMetadata() { return mBrushMetadata; }
    
    void setBrushMetadata(const BrushMetadata& metadata) { mBrushMetadata = metadata; }
    
    // Serialization
    Status createDomElement(QXmlStreamWriter& xmlStream);
    void loadDomElement(const QDomElement& element);
    
    // Re-render this stroke to a bitmap using stored metadata
    void renderToBitmap(QPainter& painter, const Object& object, qreal scale = 1.0) const;
    
private:
    BrushMetadata mBrushMetadata;
};

#endif // ENHANCEDVECTORSTROKE_H

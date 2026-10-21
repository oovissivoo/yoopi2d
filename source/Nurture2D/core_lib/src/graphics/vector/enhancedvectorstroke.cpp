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

#include "enhancedvectorstroke.h"
#include "object.h"
#include <QXmlStreamWriter>
#include <QDomElement>
#include <QPainter>
#include <QDebug>

EnhancedVectorStroke::EnhancedVectorStroke()
    : BezierCurve()
{
}

EnhancedVectorStroke::EnhancedVectorStroke(const QList<QPointF>& pointList, const QList<qreal>& pressureList, double tol, bool smooth)
    : BezierCurve(pointList, pressureList, tol, smooth)
{
}

Status EnhancedVectorStroke::createDomElement(QXmlStreamWriter& xmlStream)
{
    // First serialize the base BezierCurve
    Status status = BezierCurve::createDomElement(xmlStream);
    if (!status.ok()) return status;
    
    // Add brush metadata
    xmlStream.writeStartElement("brushMetadata");
    xmlStream.writeAttribute("toolType", QString::number(mBrushMetadata.toolType));
    xmlStream.writeAttribute("baseWidth", QString::number(mBrushMetadata.baseWidth));
    xmlStream.writeAttribute("baseFeather", QString::number(mBrushMetadata.baseFeather));
    xmlStream.writeAttribute("usePressure", mBrushMetadata.usePressure ? "true" : "false");
    xmlStream.writeAttribute("useFeather", mBrushMetadata.useFeather ? "true" : "false");
    xmlStream.writeAttribute("useAA", mBrushMetadata.useAA ? "true" : "false");
    
    // Texture settings
    xmlStream.writeAttribute("texturePreset", QString::number(mBrushMetadata.texturePreset));
    xmlStream.writeAttribute("textureSpacing", QString::number(mBrushMetadata.textureSpacing));
    xmlStream.writeAttribute("textureScatter", QString::number(mBrushMetadata.textureScatter));
    xmlStream.writeAttribute("textureRotation", QString::number(mBrushMetadata.textureRotation));
    xmlStream.writeAttribute("textureScale", QString::number(mBrushMetadata.textureScale));
    xmlStream.writeAttribute("textureFlow", QString::number(mBrushMetadata.textureFlow));
    xmlStream.writeAttribute("texturePath", mBrushMetadata.texturePath);
    
    // Pressure curves
    xmlStream.writeAttribute("pressureCurveP1X", QString::number(mBrushMetadata.pressureCurveP1.x()));
    xmlStream.writeAttribute("pressureCurveP1Y", QString::number(mBrushMetadata.pressureCurveP1.y()));
    xmlStream.writeAttribute("pressureCurveP2X", QString::number(mBrushMetadata.pressureCurveP2.x()));
    xmlStream.writeAttribute("pressureCurveP2Y", QString::number(mBrushMetadata.pressureCurveP2.y()));
    
    // Size/opacity curves
    xmlStream.writeAttribute("sizeCurveP1X", QString::number(mBrushMetadata.sizeCurveP1.x()));
    xmlStream.writeAttribute("sizeCurveP1Y", QString::number(mBrushMetadata.sizeCurveP1.y()));
    xmlStream.writeAttribute("sizeCurveP2X", QString::number(mBrushMetadata.sizeCurveP2.x()));
    xmlStream.writeAttribute("sizeCurveP2Y", QString::number(mBrushMetadata.sizeCurveP2.y()));
    xmlStream.writeAttribute("opacityCurveP1X", QString::number(mBrushMetadata.opacityCurveP1.x()));
    xmlStream.writeAttribute("opacityCurveP1Y", QString::number(mBrushMetadata.opacityCurveP1.y()));
    xmlStream.writeAttribute("opacityCurveP2X", QString::number(mBrushMetadata.opacityCurveP2.x()));
    xmlStream.writeAttribute("opacityCurveP2Y", QString::number(mBrushMetadata.opacityCurveP2.y()));
    
    xmlStream.writeEndElement(); // brushMetadata
    
    return Status::OK;
}

void EnhancedVectorStroke::loadDomElement(const QDomElement& element)
{
    // Load base BezierCurve
    BezierCurve::loadDomElement(element);
    
    // Load brush metadata
    QDomElement brushElement = element.firstChildElement("brushMetadata");
    if (!brushElement.isNull()) {
        mBrushMetadata.toolType = brushElement.attribute("toolType", "0").toInt();
        mBrushMetadata.baseWidth = brushElement.attribute("baseWidth", "1.0").toDouble();
        mBrushMetadata.baseFeather = brushElement.attribute("baseFeather", "0.0").toDouble();
        mBrushMetadata.usePressure = brushElement.attribute("usePressure", "true") == "true";
        mBrushMetadata.useFeather = brushElement.attribute("useFeather", "false") == "true";
        mBrushMetadata.useAA = brushElement.attribute("useAA", "false") == "true";
        
        // Texture settings
        mBrushMetadata.texturePreset = brushElement.attribute("texturePreset", "0").toInt();
        mBrushMetadata.textureSpacing = brushElement.attribute("textureSpacing", "0.25").toDouble();
        mBrushMetadata.textureScatter = brushElement.attribute("textureScatter", "0.05").toDouble();
        mBrushMetadata.textureRotation = brushElement.attribute("textureRotation", "0.0").toDouble();
        mBrushMetadata.textureScale = brushElement.attribute("textureScale", "1.0").toDouble();
        mBrushMetadata.textureFlow = brushElement.attribute("textureFlow", "1.0").toDouble();
        mBrushMetadata.texturePath = brushElement.attribute("texturePath", "");
        
        // Pressure curves
        mBrushMetadata.pressureCurveP1.setX(brushElement.attribute("pressureCurveP1X", "0.25").toDouble());
        mBrushMetadata.pressureCurveP1.setY(brushElement.attribute("pressureCurveP1Y", "0.0").toDouble());
        mBrushMetadata.pressureCurveP2.setX(brushElement.attribute("pressureCurveP2X", "0.75").toDouble());
        mBrushMetadata.pressureCurveP2.setY(brushElement.attribute("pressureCurveP2Y", "1.0").toDouble());
        
        // Size/opacity curves
        mBrushMetadata.sizeCurveP1.setX(brushElement.attribute("sizeCurveP1X", "0.25").toDouble());
        mBrushMetadata.sizeCurveP1.setY(brushElement.attribute("sizeCurveP1Y", "0.0").toDouble());
        mBrushMetadata.sizeCurveP2.setX(brushElement.attribute("sizeCurveP2X", "0.75").toDouble());
        mBrushMetadata.sizeCurveP2.setY(brushElement.attribute("sizeCurveP2Y", "1.0").toDouble());
        mBrushMetadata.opacityCurveP1.setX(brushElement.attribute("opacityCurveP1X", "0.25").toDouble());
        mBrushMetadata.opacityCurveP1.setY(brushElement.attribute("opacityCurveP1Y", "0.0").toDouble());
        mBrushMetadata.opacityCurveP2.setX(brushElement.attribute("opacityCurveP2X", "0.75").toDouble());
        mBrushMetadata.opacityCurveP2.setY(brushElement.attribute("opacityCurveP2Y", "1.0").toDouble());
    }
}

void EnhancedVectorStroke::renderToBitmap(QPainter& painter, const Object& object, qreal scale) const
{
    // For now, use the standard BezierCurve rendering
    // Future: implement texture stamping and advanced brush effects
    QTransform transform;
    transform.scale(scale, scale);
    
    // Use const_cast to call the non-const method from const context
    const_cast<EnhancedVectorStroke*>(this)->drawPath(painter, object, transform, false, false);
}

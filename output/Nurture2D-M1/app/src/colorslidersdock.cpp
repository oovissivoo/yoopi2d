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

#include "colorslidersdock.h"
#include "inlinecolorinspector.h"
#include "editor.h"
#include "colormanager.h"

ColorSlidersDock::ColorSlidersDock(QWidget* parent)
    : BaseDockWidget(parent)
{
    setWindowTitle(tr("Color Sliders"));
    setObjectName("colorSlidersDock");
}

void ColorSlidersDock::initUI()
{
    mColorInspector = new InlineColorInspector(this);
    setDockContentWidget(mColorInspector);
    
    connect(mColorInspector, &InlineColorInspector::colorChanged,
            this, &ColorSlidersDock::colorChanged);
    
    // Set initial color to black with red hue
    mColorInspector->setColor(QColor::fromHsv(0, 0, 0));
}

void ColorSlidersDock::updateUI()
{
    if (editor() && editor()->color())
    {
        QColor frontColor = editor()->color()->frontColor();
        setColor(frontColor);
    }
}

void ColorSlidersDock::setColor(const QColor& color)
{
    if (mColorInspector)
    {
        mColorInspector->setColor(color);
    }
}

QColor ColorSlidersDock::color() const
{
    if (mColorInspector)
    {
        return mColorInspector->color();
    }
    return Qt::black;
}

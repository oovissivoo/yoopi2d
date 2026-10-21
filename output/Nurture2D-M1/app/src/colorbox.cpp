/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include <QVBoxLayout>
#include "colorwheel.h"
#include "colorbox.h"
#include "editor.h"
#include "colormanager.h"
#include <QApplication>
#include <QSettings>


ColorBox::ColorBox(QWidget* parent) : BaseDockWidget( parent )
{
    setWindowTitle(tr("Color Box", "Color Box window title"));
}

ColorBox::~ColorBox()
{
}

void ColorBox::initUI()
{
    mColorWheel = new ColorWheel(this);
    // Removed old inspector toggle - now using ColorWheel's built-in toggle
    // Ensure ColorWheel uses the same 2f2f2f panel background as the dock
    QPalette wheelPal = mColorWheel->palette();
    wheelPal.setColor(QPalette::Window, QColor(47, 47, 47)); // 2f2f2fff
    mColorWheel->setPalette(wheelPal);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(3, 3, 3, 3);
    layout->addWidget(mColorWheel);
    layout->setStretch(0, 1);
    QWidget* mainWidget = new QWidget;
    mainWidget->setLayout(layout);
    // Ensure the ColorBox panel background matches the dock master container (2f2f2f)
    mainWidget->setAutoFillBackground(true);
    QPalette pal = mainWidget->palette();
    pal.setColor(QPalette::Window, QColor(47, 47, 47)); // 2f2f2fff
    pal.setColor(QPalette::WindowText, QColor(179, 179, 179));
    pal.setColor(QPalette::ButtonText, QColor(179, 179, 179));
    mainWidget->setPalette(pal);
    setDockContentWidget(mainWidget);

    connect(mColorWheel, &ColorWheel::colorChanged, this, &ColorBox::onWheelMove);
    connect(mColorWheel, &ColorWheel::colorSelected, this, &ColorBox::onWheelRelease);

    connect(editor(), &Editor::objectLoaded, this, &ColorBox::updateUI);

    // Initialize the wheel to black with red hue on startup (legacy behavior).
    mColorWheel->setColor(QColor::fromHsv(0, 0, 0)); // Black with red hue (0)
}

void ColorBox::toggleInspector()
{
    // Color sliders dock removed - new cosmic color sliders coming in a future release
}

void ColorBox::updateUI()
{
    // If global suppression is enabled (during app initialization), skip
    // updating the color wheel from the ColorManager so we don't overwrite
    // the user's tool color during startup.
    if (qApp->property("SuppressColorBoxUpdates").toBool()) return;

    QColor newColor = editor()->color()->frontColor();
    setColor(newColor);
}

QColor ColorBox::color()
{
    return mColorWheel->color();
}

void ColorBox::setColor(QColor newColor)
{
    // Respect global suppression flag as well
    if (qApp->property("SuppressColorBoxUpdates").toBool()) return;

    newColor = newColor.toHsv();

    if ( newColor != mColorWheel->color() )
    {
        mColorWheel->setColor(newColor);
    }
}

void ColorBox::onWheelMove(const QColor& color)
{
    emit colorChanged(color);
}

void ColorBox::onWheelRelease(const QColor& color)
{
     emit colorChanged(color);
}

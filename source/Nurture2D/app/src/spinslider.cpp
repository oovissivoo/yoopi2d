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

#include "spinslider.h"

#include <cmath>
#include <QLabel>
#include <QGridLayout>
#include <QLocale>
#include <QDebug>
#include <QStyle>

#include "../paintlyFillAlgorithm/blenderstyleslider.h"

SpinSlider::SpinSlider(QWidget* parent) : QWidget(parent)
{}

void SpinSlider::init(QString text, GROWTH_TYPE type, VALUE_TYPE dataType, qreal min, qreal max)
{
    if (type == LOG)
    {
        // important! dividing by zero is not acceptable.
        Q_ASSERT_X(min > 0.0, "SpinSlider", "Real type value must larger than 0!!");
    }

    mValue = 1.0;
    mGrowthType = type;
    mValueType = dataType;
    mMin = min;
    mMax = max;

    mLabel = new QLabel(text + ": ");

    mSlider = new BlenderStyleSlider(Qt::Horizontal, this);
    mSlider->setMinimum(0);
    mSlider->setMaximum(100);
    mSlider->setSliderColor(BlenderStyleSlider::getAccentColor());

    // Set up display value callback so BlenderStyleSlider can convert user input
    auto displayToSliderCallback = [this](qreal displayValue) -> int {
        qreal clamped = displayValue;
        if (clamped < mMin) clamped = mMin;
        if (clamped > mMax) clamped = mMax;
        int sliderValue = 0;
        if (mGrowthType == LINEAR) {
            sliderValue = qRound((mSlider->maximum() * (clamped - mMin) / (mMax - mMin)));
        } else if (mGrowthType == LOG) {
            sliderValue = qRound(std::log(clamped / mMin) * mSlider->maximum() / std::log(mMax / mMin));
        } else if (mGrowthType == EXPONENT) {
            sliderValue = qRound(std::pow((clamped - mMin) * std::pow(mSlider->maximum(), mExp) / (mMax - mMin), 1 / mExp));
        }
        return sliderValue;
    };
    mSlider->setDisplayToSliderCallback(displayToSliderCallback);

    QGridLayout* layout = new QGridLayout();
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(1);

    layout->addWidget(mLabel, 0, 0, 1, 1);
    layout->addWidget(mSlider, 1, 0, 1, 2);

    setLayout(layout);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(mSlider, &BlenderStyleSlider::valueChanged, this, &SpinSlider::onSliderValueChanged);
    connect(mSlider, &QSlider::sliderPressed, this, [this]() { emit sliderPressed(); });
    connect(mSlider, &QSlider::sliderReleased, this, [this]() { emit sliderReleased(); });
}

void SpinSlider::setSliderColor(const QColor& color)
{
    if (mSlider) mSlider->setSliderColor(color);
}

void SpinSlider::changeValue(qreal value)
{
    mValue = value;
    if (!mEmitOnReleaseOnly) {
        emit valueChanged(value);
    }
}

void SpinSlider::onSliderValueChanged(int v)
{
    qreal value2 = 0.0;
    if (mGrowthType == LINEAR)
    {
        value2 = mMin + v * (mMax - mMin) / mSlider->maximum();
    }
    else if (mGrowthType == LOG)
    {
        value2 = mMin * std::exp(v * std::log(mMax / mMin) / mSlider->maximum());
    }
    else if (mGrowthType == EXPONENT) {
        value2 = mMin + std::pow(v, mExp) * (mMax - mMin) / std::pow(mSlider->maximum(), mExp);
    }
    // Update the display value on the slider widget
    if (mSlider) {
        mSlider->setDisplayValue(value2);
    }
    changeValue(value2);
}

void SpinSlider::setLabel(QString newText)
{
    mLabel->setText(newText);
}

void SpinSlider::setValue(qreal v)
{
    Q_ASSERT(mMin <= v && v <= mMax);

    int value2 = 0;
    if (mGrowthType == LINEAR)
    {
        value2 =qRound((mSlider->maximum() * (v - mMin) / (mMax - mMin)));
    }
    else if (mGrowthType == LOG)
    {
        value2 = qRound(std::log(v / mMin) * mSlider->maximum() / std::log(mMax / mMin));
    }
    else if (mGrowthType == EXPONENT)
    {
        value2 = qRound(std::pow((v - mMin) * std::pow(mSlider->maximum(), mExp) / (mMax - mMin), 1 / mExp));
    }

    // Update internal value but do not emit if emit-on-release is enabled
    mValue = v;
    if (!mEmitOnReleaseOnly) {
        emit valueChanged(v);
    }
    // Update the display value on the slider widget
    if (mSlider) {
        mSlider->setDisplayValue(v);
    }
    mSlider->setSliderPosition(value2);
}

void SpinSlider::setPixelPos(qreal min, qreal max, int val, int space, bool upsideDown)
{
    mSlider->setSliderPosition(QStyle::sliderValueFromPosition(static_cast<int>(min),
                                                               static_cast<int>(max), val, space, upsideDown));
}

void SpinSlider::setExponent(const qreal exp)
{
    mExp = exp;
}

// Convert current value to slider steps and set as default
void SpinSlider::setDefaultToCurrent()
{
    if (!mSlider) return;
    int value2 = 0;
    if (mGrowthType == LINEAR)
    {
        value2 = qRound((mSlider->maximum() * (mValue - mMin) / (mMax - mMin)));
    }
    else if (mGrowthType == LOG)
    {
        value2 = qRound(std::log(mValue / mMin) * mSlider->maximum() / std::log(mMax / mMin));
    }
    else if (mGrowthType == EXPONENT)
    {
        value2 = qRound(std::pow((mValue - mMin) * std::pow(mSlider->maximum(), mExp) / (mMax - mMin), 1 / mExp));
    }
    mSlider->setDefaultValue(value2);
}

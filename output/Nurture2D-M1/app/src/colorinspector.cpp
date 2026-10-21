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
#include "colorinspector.h"
#include "ui_colorinspector.h"

#include <QSettings>
#include <QDebug>

#include "colorslider.h"
#include "pencildef.h"
#include "editor.h"
#include "colormanager.h"
#include "util.h"

#include "../CUI/cosmicbutton.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QIntValidator>


ColorInspector::ColorInspector(QWidget *parent) :
    BaseDockWidget(parent)
{
    QWidget* innerWidget = new QWidget;
    setWindowTitle(tr("Color Inspector", "Window title of color inspector"));

    ui = new Ui::ColorInspector;
    ui->setupUi(innerWidget);
    setDockContentWidget(innerWidget);
}

ColorInspector::~ColorInspector()
{
    delete ui;
}

void ColorInspector::initUI()
{
    mCurrentColor = editor()->color()->frontColor();

    QSettings settings(PENCIL2D, PENCIL2D);
    isRgbColors = settings.value("isRgb").toBool();

    if (isRgbColors) {
        ui->colorSpecTabWidget->setCurrentWidget(ui->rgbTab);
    } else {
        ui->colorSpecTabWidget->setCurrentWidget(ui->hsvTab);
    }
    onColorSpecChanged();

    ui->redSlider->init(ColorSlider::ColorSpecType::RGB, ColorSlider::ColorType::RED, mCurrentColor, 0.0, 255.0);
    ui->greenSlider->init(ColorSlider::ColorSpecType::RGB, ColorSlider::ColorType::GREEN, mCurrentColor, 0.0, 255.0);
    ui->blueSlider->init(ColorSlider::ColorSpecType::RGB, ColorSlider::ColorType::BLUE, mCurrentColor, 0.0, 255.0);
    ui->rgbAlphaSlider->hide();

    ui->hueSlider->init(ColorSlider::ColorSpecType::HSV, ColorSlider::ColorType::HUE, mCurrentColor, 0.0, 359.0);
    ui->saturationSlider->init(ColorSlider::ColorSpecType::HSV, ColorSlider::ColorType::SAT, mCurrentColor, 0.0, 255.0);
    ui->valueSlider->init(ColorSlider::ColorSpecType::HSV, ColorSlider::ColorType::VAL, mCurrentColor, 0.0, 255.0);
    ui->hsvAlphaSlider->hide();

    // Remove color preview area (collapse)
    ui->colorWrapper->setVisible(false);
    ui->colorWrapper->setMaximumHeight(0);
    ui->color->setVisible(false);
    ui->color->setMaximumHeight(0);

    connect(ui->colorSpecTabWidget, &QTabWidget::currentChanged, this, &ColorInspector::onColorSpecChanged);

    auto onColorChangedSlider = static_cast<void(ColorInspector::*)(const QColor&)>(&ColorInspector::onColorChanged);
    connect(ui->redSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    connect(ui->greenSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    connect(ui->blueSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    ui->rgbAlphaSlider->hide();
    connect(ui->hueSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    connect(ui->saturationSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    connect(ui->valueSlider, &ColorSlider::valueChanged, this, onColorChangedSlider);
    ui->hsvAlphaSlider->hide();

    // Replace spinboxes in the layouts with simple typed QLineEdit inputs.
    // Keep spinboxes hidden as canonical storage for existing logic.
    auto installTypedEdit = [this](QGridLayout* grid, QSpinBox* spin, QLineEdit*& edit) {
        if (!grid || !spin)
            return;

        int row = -1, col = -1, rowSpan = 1, colSpan = 1;
        const int index = grid->indexOf(spin);
        if (index < 0)
            return;
        grid->getItemPosition(index, &row, &col, &rowSpan, &colSpan);
        if (row < 0 || col < 0)
            return;

        edit = new QLineEdit(spin->parentWidget());
        edit->setAlignment(Qt::AlignCenter);
        edit->setFixedWidth(44);
        edit->setMinimumHeight(18);
        edit->setMaximumHeight(18);
        edit->setText(QString::number(spin->value()));
        edit->setValidator(new QIntValidator(spin->minimum(), spin->maximum(), edit));

        grid->removeWidget(spin);
        spin->hide();
        grid->addWidget(edit, row, col, rowSpan, colSpan);

        auto commitToSpin = [spin, edit]() {
            bool ok = false;
            int v = edit->text().trimmed().toInt(&ok);
            if (!ok) {
                QSignalBlocker b(edit);
                edit->setText(QString::number(spin->value()));
                return;
            }
            v = qBound(spin->minimum(), v, spin->maximum());
            if (spin->value() != v) {
                spin->setValue(v);
            }
        };

        connect(edit, &QLineEdit::editingFinished, this, [commitToSpin]() { commitToSpin(); });

        connect(spin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
                [edit](int v) {
                    if (!edit)
                        return;
                    QSignalBlocker b(edit);
                    edit->setText(QString::number(v));
                });
    };

    auto wrapSliderWithArrows = [this](QGridLayout* grid, ColorSlider* slider, QSpinBox* spin) {
        if (!grid || !slider || !spin)
            return;

        int row = -1, col = -1, rowSpan = 1, colSpan = 1;
        const int index = grid->indexOf(slider);
        if (index < 0)
            return;
        grid->getItemPosition(index, &row, &col, &rowSpan, &colSpan);
        if (row < 0 || col < 0)
            return;

        QWidget* wrapper = new QWidget(slider->parentWidget());
        QHBoxLayout* h = new QHBoxLayout(wrapper);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(2);

        auto makeArrow = [wrapper](const QString& iconPath) {
            auto* b = new CosmicButton(wrapper);
            b->setBordered(false);
            b->setText("");
            b->setIcon(QIcon(iconPath));
            b->setIconSize(QSize(14, 14));
            b->setFixedSize(18, 18);
            b->setAutoRepeat(true);
            b->setAutoRepeatDelay(300);
            b->setAutoRepeatInterval(50);
            return b;
        };

        CosmicButton* left = makeArrow(":/Interaction/ArrowLeft.svg");
        CosmicButton* right = makeArrow(":/Interaction/ArrowRight.svg");

        grid->removeWidget(slider);
        slider->setParent(wrapper);
        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        slider->setFixedHeight(18);

        h->addWidget(left);
        h->addWidget(slider, 1);
        h->addWidget(right);
        grid->addWidget(wrapper, row, col, rowSpan, colSpan);

        connect(left, &QPushButton::clicked, this, [spin]() {
            spin->setValue(qBound(spin->minimum(), spin->value() - 1, spin->maximum()));
        });
        connect(right, &QPushButton::clicked, this, [spin]() {
            spin->setValue(qBound(spin->minimum(), spin->value() + 1, spin->maximum()));
        });
    };

    installTypedEdit(ui->rgbLayout, ui->redSpinBox, mRedEdit);
    installTypedEdit(ui->rgbLayout, ui->greenSpinBox, mGreenEdit);
    installTypedEdit(ui->rgbLayout, ui->blueSpinBox, mBlueEdit);
    ui->rgbAlphaSpinBox->hide();
    ui->rgbAlphaLabel->hide();

    // HSV edits: keep pure numeric display (no degree/percent symbols)
    installTypedEdit(ui->hsvLayout, ui->hueSpinBox, mHueEdit);
    installTypedEdit(ui->hsvLayout, ui->saturationSpinBox, mSaturationEdit);
    installTypedEdit(ui->hsvLayout, ui->valueSpinBox, mValueEdit);
    ui->hsvAlphaSpinBox->hide();
    ui->hsvAlphaLabel->hide();

    wrapSliderWithArrows(ui->rgbLayout, ui->redSlider, ui->redSpinBox);
    wrapSliderWithArrows(ui->rgbLayout, ui->greenSlider, ui->greenSpinBox);
    wrapSliderWithArrows(ui->rgbLayout, ui->blueSlider, ui->blueSpinBox);
    wrapSliderWithArrows(ui->hsvLayout, ui->hueSlider, ui->hueSpinBox);
    wrapSliderWithArrows(ui->hsvLayout, ui->saturationSlider, ui->saturationSpinBox);
    wrapSliderWithArrows(ui->hsvLayout, ui->valueSlider, ui->valueSpinBox);

    // Keep the existing onColorChanged() flow driven from spinboxes (even though they are hidden).
    auto spinBoxChanged = static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged);
    auto onColorChangedSpinBox = static_cast<void(ColorInspector::*)()>(&ColorInspector::onColorChanged);
    connect(ui->redSpinBox, spinBoxChanged, this, onColorChangedSpinBox);
    clearFocusOnFinished(ui->redSpinBox);
    connect(ui->greenSpinBox, spinBoxChanged, this, onColorChangedSpinBox);
    clearFocusOnFinished(ui->greenSpinBox);
    connect(ui->blueSpinBox, spinBoxChanged, this, onColorChangedSpinBox);
    clearFocusOnFinished(ui->blueSpinBox);
    connect(ui->hueSpinBox, spinBoxChanged, this, onColorChangedSpinBox);
    clearFocusOnFinished(ui->hueSpinBox);
    connect(ui->saturationSpinBox, spinBoxChanged, this, onColorChangedSpinBox);
    clearFocusOnFinished(ui->saturationSpinBox);
    connect(ui->valueSpinBox, spinBoxChanged, this, onColorChangedSpinBox);
    clearFocusOnFinished(ui->valueSpinBox);

    connect(editor(), &Editor::objectLoaded, this, &ColorInspector::updateUI);
}

void ColorInspector::updateUI()
{
    QColor newColor = editor()->color()->frontColor();
    setColor(newColor);
}

void ColorInspector::setColor(QColor newColor)
{
    // compare under the same color spec
    newColor = (isRgbColors) ? newColor.toRgb() : newColor.toHsv();

    if (newColor == mCurrentColor)
    {
        return;
    }

    mCurrentColor = newColor;

    updateControls();
}

QColor ColorInspector::color()
{
    return mCurrentColor;
}

void ColorInspector::updateControls()
{
    // this is a UI update function, never emit any signals
    // grab the color from color manager, and then update itself, that's it.

    QSignalBlocker b1(ui->redSlider);
    QSignalBlocker b2(ui->greenSlider);
    QSignalBlocker b3(ui->blueSlider);
    QSignalBlocker b4(ui->rgbAlphaSlider);

    ui->redSlider->setRgb(mCurrentColor);
    ui->greenSlider->setRgb(mCurrentColor);
    ui->blueSlider->setRgb(mCurrentColor);
    ui->redSlider->setFixedHeight(18);
    ui->greenSlider->setFixedHeight(18);
    ui->blueSlider->setFixedHeight(18);
    ui->rgbAlphaSlider->hide();

    QSignalBlocker b5(ui->redSpinBox);
    QSignalBlocker b6(ui->greenSpinBox);
    QSignalBlocker b7(ui->blueSpinBox);
    QSignalBlocker b8(ui->rgbAlphaSpinBox);

    ui->redSpinBox->setValue(mCurrentColor.red());
    ui->greenSpinBox->setValue(mCurrentColor.green());
    ui->blueSpinBox->setValue(mCurrentColor.blue());
    ui->rgbAlphaSpinBox->hide();

    if (mRedEdit) {
        QSignalBlocker b(mRedEdit);
        mRedEdit->setText(QString::number(ui->redSpinBox->value()));
    }
    if (mGreenEdit) {
        QSignalBlocker b(mGreenEdit);
        mGreenEdit->setText(QString::number(ui->greenSpinBox->value()));
    }
    if (mBlueEdit) {
        QSignalBlocker b(mBlueEdit);
        mBlueEdit->setText(QString::number(ui->blueSpinBox->value()));
    }

    QSignalBlocker b9(ui->hueSlider);
    QSignalBlocker b10(ui->saturationSlider);
    QSignalBlocker b11(ui->valueSlider);
    QSignalBlocker b12(ui->hsvAlphaSlider);

    ui->hueSlider->setHsv(mCurrentColor);
    ui->saturationSlider->setHsv(mCurrentColor);
    ui->valueSlider->setHsv(mCurrentColor);
    ui->hueSlider->setFixedHeight(18);
    ui->saturationSlider->setFixedHeight(18);
    ui->valueSlider->setFixedHeight(18);
    ui->hsvAlphaSlider->hide();

    QSignalBlocker b13(ui->hueSpinBox);
    QSignalBlocker b14(ui->saturationSpinBox);
    QSignalBlocker b15(ui->valueSpinBox);
    QSignalBlocker b16(ui->hsvAlphaSpinBox);

    ui->hueSpinBox->setValue(mCurrentColor.hue());
    ui->saturationSpinBox->setValue(qRound(mCurrentColor.saturation() / 2.55));
    ui->valueSpinBox->setValue(qRound(mCurrentColor.value() / 2.55));
    ui->hsvAlphaSpinBox->hide();

    if (mHueEdit) {
        QSignalBlocker b(mHueEdit);
        mHueEdit->setText(QString::number(ui->hueSpinBox->value()));
    }
    if (mSaturationEdit) {
        QSignalBlocker b(mSaturationEdit);
        mSaturationEdit->setText(QString::number(ui->saturationSpinBox->value()));
    }
    if (mValueEdit) {
        QSignalBlocker b(mValueEdit);
        mValueEdit->setText(QString::number(ui->valueSpinBox->value()));
    }

    QPalette p = ui->color->palette();
    p.setColor(QPalette::Window, mCurrentColor);
    ui->color->setPalette(p);

    update();
}

void ColorInspector::onColorSpecChanged()
{
    // assume hsv if not checked
    isRgbColors = ui->colorSpecTabWidget->currentWidget() == ui->rgbTab;

    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("isRgb", isRgbColors);

    if (isRgbColors)
    {
        mCurrentColor = mCurrentColor.toRgb();
    }
    else
    {
        mCurrentColor = mCurrentColor.toHsv();
    }

    updateControls();

    emit colorSpecChanged(isRgbColors);
}

void ColorInspector::onColorChanged()
{
    if (isRgbColors) {
        mCurrentColor.setRgb(
            ui->redSpinBox->value(),
            ui->greenSpinBox->value(),
            ui->blueSpinBox->value(),
            255);
    } else {
        mCurrentColor.setHsv(
            ui->hueSpinBox->value(),
            static_cast<int>(ui->saturationSpinBox->value() * 2.55),
            static_cast<int>(ui->valueSpinBox->value() * 2.55),
            255);
    }

    updateControls();

    emit colorChanged(mCurrentColor);
}

void ColorInspector::onColorChanged(const QColor& color)
{
    mCurrentColor = color;

    updateControls();

    emit colorChanged(color);
}

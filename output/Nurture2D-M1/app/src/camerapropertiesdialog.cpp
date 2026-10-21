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

#include "camerapropertiesdialog.h"
#include "ui_camerapropertiesdialog.h"

#include "../CUI/cosmicinputfield.h"

CameraPropertiesDialog::CameraPropertiesDialog(const QString& name, int width, int height) :
    QDialog(),
    ui(new Ui::CameraPropertiesDialog)
{
    ui->setupUi(this);

    auto replaceSpinBoxWithCosmic = [](QSpinBox* oldSpin, CosmicInputField*& outField) {
        if (!oldSpin || outField)
            return;
        QWidget* parentWidget = oldSpin->parentWidget();
        if (!parentWidget || !parentWidget->layout())
            return;

        outField = new CosmicInputField(parentWidget);
        outField->setVariant(CosmicInputField::Minimal);
        outField->setRange(oldSpin->minimum(), oldSpin->maximum());
        outField->setDecimals(0);
        outField->setSingleStep(oldSpin->singleStep());
        outField->setFixedHeight(24);
        outField->setValue(oldSpin->value());
        outField->setToolTip(oldSpin->toolTip());

        if (auto* box = qobject_cast<QBoxLayout*>(parentWidget->layout())) {
            const int idx = box->indexOf(oldSpin);
            box->removeWidget(oldSpin);
            oldSpin->hide();
            if (idx >= 0) {
                box->insertWidget(idx, outField);
            } else {
                box->addWidget(outField);
            }
            return;
        }

        if (auto* grid = qobject_cast<QGridLayout*>(parentWidget->layout())) {
            int row = -1, col = -1, rowSpan = 1, colSpan = 1;
            const int idx = grid->indexOf(oldSpin);
            if (idx >= 0) {
                grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
            }
            grid->removeWidget(oldSpin);
            oldSpin->hide();
            if (row >= 0 && col >= 0) {
                grid->addWidget(outField, row, col, rowSpan, colSpan);
            } else {
                grid->addWidget(outField);
            }
        }
    };

    replaceSpinBoxWithCosmic(ui->widthBox, mWidthField);
    replaceSpinBoxWithCosmic(ui->heightBox, mHeightField);

    ui->nameBox->setText(name);
    if (mWidthField) {
        mWidthField->setValue(width);
    } else {
        ui->widthBox->setValue(width);
    }
    if (mHeightField) {
        mHeightField->setValue(height);
    } else {
        ui->heightBox->setValue(height);
    }
}

CameraPropertiesDialog::~CameraPropertiesDialog()
{
    delete ui;
}

QString CameraPropertiesDialog::getName()
{
    return ui->nameBox->text();
}

void CameraPropertiesDialog::setName(const QString& name)
{
    ui->nameBox->setText(name);
}

int CameraPropertiesDialog::getWidth()
{
    return mWidthField ? static_cast<int>(mWidthField->value()) : ui->widthBox->value();
}

void CameraPropertiesDialog::setWidth(int width)
{
    if (mWidthField) {
        mWidthField->setValue(width);
    } else {
        ui->widthBox->setValue(width);
    }
}

int CameraPropertiesDialog::getHeight()
{
    return mHeightField ? static_cast<int>(mHeightField->value()) : ui->heightBox->value();
}

void CameraPropertiesDialog::setHeight(int height)
{
    if (mHeightField) {
        mHeightField->setValue(height);
    } else {
        ui->heightBox->setValue(height);
    }
}

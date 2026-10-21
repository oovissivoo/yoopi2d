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
#include "importpositiondialog.h"
#include "ui_importpositiondialog.h"

#include <QSettings>
#include <QStandardItemModel>
#include "editor.h"
#include "layermanager.h"
#include "../CUI/cosmicdropdown.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

ImportPositionDialog::ImportPositionDialog(Editor* editor, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImportPositionDialog), mEditor(editor)
{
    ui->setupUi(this);
    mHasCameraLayer = (mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex()) != nullptr);
    // Replace with CosmicDropDown and filter camera options if absent
    if (!mPositionDropDown && ui->cbImagePosition) {
        mPositionDropDown = new CosmicDropDown(this);
        QStringList opts;
        opts << tr("Center of current view") << tr("Center of canvas (0,0)");
        if (mHasCameraLayer) {
            opts << tr("Center of camera, current frame") << tr("Center of camera, follow camera");
        }
        mPositionDropDown->setOptions(opts);
        mPositionDropDown->setAccentColor(BlenderStyleSlider::getAccentColor());
        if (auto layoutHost = this->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_2"))) {
            int idx = layoutHost->indexOf(ui->cbImagePosition);
            if (idx < 0) idx = layoutHost->count();
            layoutHost->insertWidget(idx, mPositionDropDown);
            ui->cbImagePosition->hide();
        } else {
            ui->cbImagePosition->parentWidget()->layout()->addWidget(mPositionDropDown);
            ui->cbImagePosition->hide();
        }
        connect(mPositionDropDown, &CosmicDropDown::currentIndexChanged, this, &ImportPositionDialog::didChangeComboBoxIndex);
    }
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ImportPositionDialog::changeImportView);

    QSettings settings(PENCIL2D, PENCIL2D);
    const int savedTypeInt = settings.value(IMPORT_REPOSITION_TYPE).toInt();
    ImportImageConfig::PositionType savedType = static_cast<ImportImageConfig::PositionType>(savedTypeInt);
    const int idx = indexFromType(savedType, mHasCameraLayer);
    if (mPositionDropDown) mPositionDropDown->setCurrentIndex(idx);
    didChangeComboBoxIndex(idx);
}

ImportPositionDialog::~ImportPositionDialog()
{
    delete ui;
}

void ImportPositionDialog::didChangeComboBoxIndex(const int index)
{
    // Map based on filtered options
    if (mHasCameraLayer) {
        mImportConfig.positionType = getTypeFromIndex(index);
    } else {
        // Only first two options exist
        mImportConfig.positionType = getTypeFromIndex(index);
    }
}

void ImportPositionDialog::changeImportView()
{
    if (mImportConfig.positionType == ImportImageConfig::CenterOfCamera) {
        mImportConfig.importFrame = mEditor->currentFrame();
    }

    QSettings settings(PENCIL2D, PENCIL2D);
    // Persist semantic type value to avoid index meaning flip when camera appears/disappears
    settings.setValue(IMPORT_REPOSITION_TYPE, static_cast<int>(mImportConfig.positionType));
}

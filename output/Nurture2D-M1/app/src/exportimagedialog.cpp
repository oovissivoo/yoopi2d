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

#include "exportimagedialog.h"
#include "ui_exportimageoptions.h"
#include "util.h"
#include "../CUI/cosmicdropdown.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

ExportImageDialog::ExportImageDialog(QWidget* parent, FileType eFileType) :
    ImportExportDialog(parent, ImportExportDialog::Export, eFileType),
    ui(new Ui::ExportImageOptions)
{
    ui->setupUi(getOptionsGroupBox());
    adjustDialogSize();
    if (eFileType == FileType::IMAGE_SEQUENCE)
    {
        setWindowTitle(tr("Export image sequence"));
    }
    else
    {
        setWindowTitle(tr("Export image"));
        ui->frameRangeGroupBox->hide();
    }

    // Replace format combo with CosmicDropDown
    if (!mFormatDropDown && ui->formatComboBox) {
        mFormatDropDown = new CosmicDropDown(this);
        mFormatDropDown->setOptions({QStringLiteral("PNG"), QStringLiteral("JPG"), QStringLiteral("BMP"), QStringLiteral("TIFF"), QStringLiteral("WEBP")});
        if (auto layoutHost = this->findChild<QVBoxLayout*>(QStringLiteral("verticalLayout_2"))) {
            int idx = layoutHost->indexOf(ui->formatComboBox);
            if (idx < 0) idx = layoutHost->count();
            layoutHost->insertWidget(idx, mFormatDropDown);
            ui->formatComboBox->hide();
        } else {
            ui->formatComboBox->parentWidget()->layout()->addWidget(mFormatDropDown);
            ui->formatComboBox->hide();
        }
        connect(mFormatDropDown, &CosmicDropDown::currentIndexChanged, this, [this](int){ formatChanged(getExportFormat()); });
    }
    // Export Specific mode dropdown (Keyframes / Cells Only) for image sequences
    if (!mExportSpecificModeDropDown) {
        QWidget* placeholder = this->findChild<QWidget*>(QStringLiteral("exportSpecificModePlaceholder"));
        if (placeholder) {
            mExportSpecificModeDropDown = new CosmicDropDown(this);
            mExportSpecificModeDropDown->setOptions({QStringLiteral("Keyframes"), QStringLiteral("Cells Only")});
            mExportSpecificModeDropDown->setCurrentIndex(0);

            if (auto grid = this->findChild<QGridLayout*>(QStringLiteral("gridLayout_2"))) {
                int row = 1;
                int col = 1;
                grid->addWidget(mExportSpecificModeDropDown, row, col);
            } else if (placeholder->parentWidget() && placeholder->parentWidget()->layout()) {
                placeholder->parentWidget()->layout()->addWidget(mExportSpecificModeDropDown);
            }
            placeholder->hide();

            auto updateTooltip = [this](int index) {
                if (!mExportSpecificModeDropDown) return;
                if (index == 0) {
                    mExportSpecificModeDropDown->setToolTip(tr("Export only animation keyframes for the active bitmap layer, named after the timeline frame they are on."));
                } else {
                    mExportSpecificModeDropDown->setToolTip(tr("Export only the unique cells (drawings) used in the active bitmap layer, each once, at full canvas size."));
                }
            };
            updateTooltip(mExportSpecificModeDropDown->currentIndex());
            connect(mExportSpecificModeDropDown, &CosmicDropDown::currentIndexChanged, this, updateTooltip);
        }
    }

    formatChanged(getExportFormat()); // Make sure file extension matches format dropdown
    adjustDialogSize();
}

ExportImageDialog::~ExportImageDialog()
{
    delete ui;
}

// Removed setCamerasInfo: camera selection no longer part of dialog

// Removed setExportSize: resolution controls removed from UI

// Removed getExportSize: resolution controls removed from UI

bool ExportImageDialog::getTransparency() const
{
    return ui->cbTransparency->checkState() == Qt::Checked;
}

QString ExportImageDialog::getExportFormat() const
{
    if (mFormatDropDown) return mFormatDropDown->currentText();
    return QString();
}

bool ExportImageDialog::isExportSpecificEnabled() const
{
    if (auto cb = this->findChild<QCheckBox*>(QStringLiteral("cbExportSpecific"))) {
        return cb->isChecked();
    }
    return false;
}

ExportImageDialog::ExportSpecificMode ExportImageDialog::getExportSpecificMode() const
{
    int idx = mExportSpecificModeDropDown ? mExportSpecificModeDropDown->currentIndex() : 0;
    return (idx == 1) ? ExportSpecificMode::CellsOnly : ExportSpecificMode::Keyframes;
}

// Removed getCameraLayerName: camera selection removed from UI

void ExportImageDialog::formatChanged(const QString& format)
{
    setFileExtension(format.toLower());
    setTransparencyOptionVisibility(format);
}

// Removed cameraComboChanged: obsolete

void ExportImageDialog::setTransparencyOptionVisibility(const QString &format)
{
    if (format == "JPG" || format == "BMP")
        ui->cbTransparency->setDisabled(true);
    else
        ui->cbTransparency->setDisabled(false);
}

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

#include "exportmoviedialog.h"
#include "ui_exportmovieoptions.h"
#include "util.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicinputfield.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

ExportMovieDialog::ExportMovieDialog(QWidget *parent, Mode mode, FileType fileType) :
    ImportExportDialog(parent, mode, fileType),
    ui(new Ui::ExportMovieOptions)
{
    ui->setupUi(getOptionsGroupBox());
    adjustDialogSize();

    auto replaceSpinBoxWithCosmic = [this](QSpinBox* oldSpin, CosmicInputField*& outField) {
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

    // Resolution group
    replaceSpinBoxWithCosmic(ui->widthSpinBox, mWidthField);
    replaceSpinBoxWithCosmic(ui->heightSpinBox, mHeightField);
    // Range group
    replaceSpinBoxWithCosmic(ui->startSpinBox, mStartField);
    replaceSpinBoxWithCosmic(ui->endSpinBox, mEndField);
    // Replace camera combo with CosmicDropDown
    if (!mCameraDropDown && ui->cameraCombo) {
        mCameraDropDown = new CosmicDropDown(this);
        if (auto layoutHost = this->findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_3"))) {
            int idx = layoutHost->indexOf(ui->cameraCombo);
            if (idx < 0) idx = layoutHost->count();
            layoutHost->insertWidget(idx, mCameraDropDown);
            ui->cameraCombo->hide();
        } else {
            ui->cameraCombo->parentWidget()->layout()->addWidget(mCameraDropDown);
            ui->cameraCombo->hide();
        }
    }

    if (fileType == FileType::GIF) {
        setWindowTitle(tr("Export Animated GIF"));
        ui->exporterGroupBox->hide();
    } else {
        setWindowTitle(tr("Export Movie"));
    }

    QSizePolicy policy = ui->unevenWidthLabel->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    ui->unevenWidthLabel->setSizePolicy(policy);
    policy = ui->unevenHeightLabel->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    ui->unevenHeightLabel->setSizePolicy(policy);

    connect(this, &ExportMovieDialog::filePathsChanged, this, &ExportMovieDialog::onFilePathsChanged);
    if (mWidthField) {
        connect(mWidthField, &CosmicInputField::valueChanged, this, [this](double) { validateResolution(); });
    } else {
        connect(ui->widthSpinBox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ExportMovieDialog::validateResolution);
    }
    if (mHeightField) {
        connect(mHeightField, &CosmicInputField::valueChanged, this, [this](double) { validateResolution(); });
    } else {
        connect(ui->heightSpinBox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ExportMovieDialog::validateResolution);
    }
    adjustDialogSize();
}

ExportMovieDialog::~ExportMovieDialog()
{
    delete ui;
}

void ExportMovieDialog::setCamerasInfo(const std::vector<std::pair<QString, QSize>> camerasInfo)
{
    mCameraData = camerasInfo;
    if (mCameraDropDown) {
        QStringList names;
        for (const auto& camera : camerasInfo) names << camera.first;
        mCameraDropDown->setOptions(names);
        QObject::disconnect(mCameraDropDown, nullptr, this, nullptr);
        connect(mCameraDropDown, &CosmicDropDown::currentIndexChanged, this, &ExportMovieDialog::updateResolutionCombo);
        updateResolutionCombo(0);
    }
}

void ExportMovieDialog::updateResolutionCombo( int index )
{
    QSize camSize;
    if (index >= 0 && index < static_cast<int>(mCameraData.size())) {
        camSize = mCameraData[static_cast<size_t>(index)].second;
    }

    if (mWidthField) {
        QSignalBlocker b1(mWidthField);
        mWidthField->setValue(camSize.width());
    } else {
        QSignalBlocker b1(ui->widthSpinBox);
        ui->widthSpinBox->setValue(camSize.width());
    }
    if (mHeightField) {
        QSignalBlocker b2(mHeightField);
        mHeightField->setValue(camSize.height());
    } else {
        QSignalBlocker b2(ui->heightSpinBox);
        ui->heightSpinBox->setValue(camSize.height());
    }
    validateResolution();
}

void ExportMovieDialog::setDefaultRange(int startFrame, int endFrame, int endFrameWithSounds)
{
    mEndFrame = endFrame;
    mEndFrameWithSounds = endFrameWithSounds;

    if (mStartField) {
        QSignalBlocker b1(mStartField);
        mStartField->setValue(startFrame);
    } else {
        QSignalBlocker b1(ui->startSpinBox);
        ui->startSpinBox->setValue(startFrame);
    }
    if (mEndField) {
        QSignalBlocker b2(mEndField);
        mEndField->setValue(endFrame);
    } else {
        QSignalBlocker b2(ui->endSpinBox);
        ui->endSpinBox->setValue(endFrame);
    }

    connect(ui->frameCheckBox, &QCheckBox::clicked, this, &ExportMovieDialog::frameCheckboxClicked);
}

QString ExportMovieDialog::getSelectedCameraName()
{
    if (mCameraDropDown) return mCameraDropDown->currentText();
    return QString();
}

QSize ExportMovieDialog::getExportSize()
{
    const int w = mWidthField ? static_cast<int>(mWidthField->value()) : ui->widthSpinBox->value();
    const int h = mHeightField ? static_cast<int>(mHeightField->value()) : ui->heightSpinBox->value();
    return QSize(w, h);
}

bool ExportMovieDialog::getTransparency() const
{
    return ui->transparencyCheckBox->isChecked() && supportsTransparency(getFilePath());
}

int ExportMovieDialog::getStartFrame()
{
    return mStartField ? static_cast<int>(mStartField->value()) : ui->startSpinBox->value();
}

int ExportMovieDialog::getEndFrame()
{
    return mEndField ? static_cast<int>(mEndField->value()) : ui->endSpinBox->value();
}

bool ExportMovieDialog::getLoop()
{
    return ui->loopCheckBox->isChecked();
}

void ExportMovieDialog::frameCheckboxClicked(bool checked)
{
    int e = (checked) ? mEndFrameWithSounds : mEndFrame;
    if (mEndField) {
        mEndField->setValue(e);
    } else {
        ui->endSpinBox->setValue(e);
    }
}

void ExportMovieDialog::onFilePathsChanged(QStringList filePaths)
{
    QString filePath = filePaths.first().toLower();
    bool canLoop = supportsLooping(filePath);
    ui->loopCheckBox->setEnabled(canLoop);
    if (!canLoop)
    {
        ui->loopCheckBox->setChecked(false);
    }
    ui->transparencyCheckBox->setEnabled(supportsTransparency(filePath));
    validateResolution();
}

bool ExportMovieDialog::supportsLooping(QString filePath) const
{
    return filePath.endsWith(".apng", Qt::CaseInsensitive) ||
           filePath.endsWith(".gif", Qt::CaseInsensitive);
}

bool ExportMovieDialog::supportsTransparency(QString filePath) const
{
    return filePath.endsWith(".apng", Qt::CaseInsensitive) ||
           filePath.endsWith(".webm", Qt::CaseInsensitive);
}

void ExportMovieDialog::validateResolution()
{
    const bool isMp4 = getFilePath().endsWith(".mp4", Qt::CaseInsensitive);
    const int w = mWidthField ? static_cast<int>(mWidthField->value()) : ui->widthSpinBox->value();
    const int h = mHeightField ? static_cast<int>(mHeightField->value()) : ui->heightSpinBox->value();
    const bool widthValid = !isMp4 || w % 2 == 0;
    const bool heightValid = !isMp4 || h % 2 == 0;
    ui->unevenWidthLabel->setHidden(widthValid);
    ui->unevenHeightLabel->setHidden(heightValid);
    setOkButtonEnabled(widthValid && heightValid);
}

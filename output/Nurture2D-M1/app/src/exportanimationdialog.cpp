#include "exportanimationdialog.h"
#include "ui_exportanimationoptions.h"
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QDir>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/blenderstyletoggle.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"

ExportAnimationDialog::ExportAnimationDialog(QWidget* parent)
    : ImportExportDialog(parent, ImportExportDialog::Export, FileType::IMAGE_SEQUENCE), ui(new Ui::ExportAnimationOptions)
{
    ui->setupUi(getOptionsGroupBox());
    setWindowTitle(tr("Export animation"));

    // Replace resolution spinboxes with CosmicInputField (unified numeric template)
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

    replaceSpinBoxWithCosmic(ui->imgWidthSpinBox, mImgWidthField);
    replaceSpinBoxWithCosmic(ui->imgHeightSpinBox, mImgHeightField);

    // Hide top-level file settings; we manage destination in this dialog
    hideFileGroupBox(true);
    adjustDialogSize();

    connect(ui->destBrowseButton, &QPushButton::clicked, this, &ExportAnimationDialog::onBrowseDest);

    // Replace format combo with CosmicDropDown
    if (!mFormatDropDown && ui->formatComboBox) {
        mFormatDropDown = new CosmicDropDown(this);
        mFormatDropDown->setOptions({QStringLiteral("PNG"), QStringLiteral("JPG"), QStringLiteral("BMP"), QStringLiteral("TIFF"), QStringLiteral("WEBP")});
        if (auto layoutHost = this->findChild<QHBoxLayout*>(QStringLiteral("formatLayout"))) {
            int idx = layoutHost->indexOf(ui->formatComboBox);
            if (idx < 0) idx = layoutHost->count();
            layoutHost->insertWidget(idx, mFormatDropDown);
            ui->formatComboBox->hide();
        } else {
            ui->formatComboBox->parentWidget()->layout()->addWidget(mFormatDropDown);
            ui->formatComboBox->hide();
        }
    }
    
    // Active Layer toggle setup
    if (!mActiveLayerToggle && this->findChild<QWidget*>(QStringLiteral("activeLayerTogglePlaceholder"))) {
        QWidget* placeholder = this->findChild<QWidget*>(QStringLiteral("activeLayerTogglePlaceholder"));
        mActiveLayerToggle = new BlenderStyleToggle(this);
        mActiveLayerToggle->setText(tr("Active Layer"));
        mActiveLayerToggle->setChecked(false);
        mActiveLayerToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        if (auto layoutHost = this->findChild<QVBoxLayout*>(QStringLiteral("activeLayerLayout"))) {
            int idx = layoutHost->indexOf(placeholder);
            if (idx < 0) idx = layoutHost->count();
            layoutHost->insertWidget(idx, mActiveLayerToggle);
            placeholder->hide();
        } else if (placeholder && placeholder->parentWidget() && placeholder->parentWidget()->layout()) {
            placeholder->parentWidget()->layout()->addWidget(mActiveLayerToggle);
            placeholder->hide();
        }
        connect(mActiveLayerToggle, &BlenderStyleToggle::toggled, this, &ExportAnimationDialog::onActiveLayerToggled);
    }

    // Create Parent Folder toggle setup
    if (!mCreateParentFolderToggle && this->findChild<QWidget*>(QStringLiteral("createParentFolderTogglePlaceholder"))) {
        QWidget* placeholder = this->findChild<QWidget*>(QStringLiteral("createParentFolderTogglePlaceholder"));
        mCreateParentFolderToggle = new BlenderStyleToggle(this);
        mCreateParentFolderToggle->setText(tr("Create Parent Folder"));
        mCreateParentFolderToggle->setChecked(true);
        mCreateParentFolderToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        if (auto grid = this->findChild<QGridLayout*>(QStringLiteral("gridLayout"))) {
            // Place at row 0, column 1
            grid->addWidget(mCreateParentFolderToggle, 0, 1);
            placeholder->hide();
        } else if (placeholder && placeholder->parentWidget() && placeholder->parentWidget()->layout()) {
            placeholder->parentWidget()->layout()->addWidget(mCreateParentFolderToggle);
            placeholder->hide();
        }
        // Show/hide parent folder name controls based on toggle
        connect(mCreateParentFolderToggle, &BlenderStyleToggle::toggled, this, [this](bool checked) {
            if (ui->parentFolderNameEdit) ui->parentFolderNameEdit->setVisible(checked);
            if (auto lbl = this->findChild<QLabel*>(QStringLiteral("parentFolderNameLabel"))) lbl->setVisible(checked);
            adjustDialogSize();
        });
    }

    // Export Specific mode dropdown (Keyframes / Cells Only)
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

            // Tooltips per mode (update widget tooltip when selection changes)
            auto updateTooltip = [this](int index) {
                if (!mExportSpecificModeDropDown) return;
                if (index == 0) {
                    mExportSpecificModeDropDown->setToolTip(tr("Export only animation keyframes, named after the timeline frame they are on."));
                } else {
                    mExportSpecificModeDropDown->setToolTip(tr("Export only the unique cells (drawings) used in the animation, each once, at full canvas size."));
                }
            };
            updateTooltip(mExportSpecificModeDropDown->currentIndex());
            connect(mExportSpecificModeDropDown, &CosmicDropDown::currentIndexChanged, this, updateTooltip);
        }
    }

    // Initialize visibility
    onActiveLayerToggled(false);
    if (ui->parentFolderNameEdit) ui->parentFolderNameEdit->setVisible(mCreateParentFolderToggle ? mCreateParentFolderToggle->isChecked() : true);
    if (auto lbl = this->findChild<QLabel*>(QStringLiteral("parentFolderNameLabel"))) lbl->setVisible(mCreateParentFolderToggle ? mCreateParentFolderToggle->isChecked() : true);

    adjustDialogSize();
}

ExportAnimationDialog::~ExportAnimationDialog()
{
    delete ui;
}

void ExportAnimationDialog::setResolution(const QSize& size)
{
    if (mImgWidthField) {
        mImgWidthField->setValue(size.width());
    } else {
        ui->imgWidthSpinBox->setValue(size.width());
    }
    if (mImgHeightField) {
        mImgHeightField->setValue(size.height());
    } else {
        ui->imgHeightSpinBox->setValue(size.height());
    }
}

QSize ExportAnimationDialog::getExportSize() const
{
    const int w = mImgWidthField ? static_cast<int>(mImgWidthField->value()) : ui->imgWidthSpinBox->value();
    const int h = mImgHeightField ? static_cast<int>(mImgHeightField->value()) : ui->imgHeightSpinBox->value();
    return QSize(w, h);
}

QString ExportAnimationDialog::getExportFormat() const
{
    if (mFormatDropDown) return mFormatDropDown->currentText();
    return QString();
}

bool ExportAnimationDialog::getTransparency() const
{
    return ui->cbTransparency->isChecked();
}

QString ExportAnimationDialog::getBaseFolderName() const
{
    // Backward compatibility: map to new parent folder name field
    return ui->parentFolderNameEdit ? ui->parentFolderNameEdit->text() : QString();
}

QString ExportAnimationDialog::getDestinationDir() const
{
    return ui->destDirEdit->text();
}

void ExportAnimationDialog::onBrowseDest()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select destination"), QDir::homePath());
    if (!dir.isEmpty()) {
        ui->destDirEdit->setText(dir);
    }
}

// New getters
bool ExportAnimationDialog::isActiveLayerMode() const
{
    return mActiveLayerToggle ? mActiveLayerToggle->isChecked() : false;
}

bool ExportAnimationDialog::shouldCreateParentFolder() const
{
    return mCreateParentFolderToggle ? mCreateParentFolderToggle->isChecked() : true;
}

QString ExportAnimationDialog::getParentFolderName() const
{
    return ui->parentFolderNameEdit ? ui->parentFolderNameEdit->text() : QString();
}

bool ExportAnimationDialog::isExportSpecificEnabled() const
{
    if (auto cb = this->findChild<QCheckBox*>(QStringLiteral("cbExportSpecific"))) {
        return cb->isChecked();
    }
    return false;
}

ExportAnimationDialog::ExportSpecificMode ExportAnimationDialog::getExportSpecificMode() const
{
    int idx = mExportSpecificModeDropDown ? mExportSpecificModeDropDown->currentIndex() : 0;
    return (idx == 1) ? ExportSpecificMode::CellsOnly : ExportSpecificMode::Keyframes;
}

void ExportAnimationDialog::onActiveLayerToggled(bool checked)
{
    // Hide or show controls based on Active Layer mode
    if (ui->resolutionGroup) ui->resolutionGroup->setVisible(!checked);
    // Keep keyframes-only option visible in both modes

    // Parent folder controls
    if (mCreateParentFolderToggle) mCreateParentFolderToggle->setVisible(!checked);
    if (auto lblCreate = this->findChild<QLabel*>(QStringLiteral("createParentFolderLabel"))) lblCreate->setVisible(!checked);

    const bool showParentName = (!checked) && (mCreateParentFolderToggle ? mCreateParentFolderToggle->isChecked() : true);
    if (ui->parentFolderNameEdit) ui->parentFolderNameEdit->setVisible(showParentName);
    if (auto lblParent = this->findChild<QLabel*>(QStringLiteral("parentFolderNameLabel"))) lblParent->setVisible(showParentName);

    adjustDialogSize();
}
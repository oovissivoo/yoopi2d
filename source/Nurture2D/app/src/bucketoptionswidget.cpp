/*
Pencil2D - Traditional Animation Software
... (license retained)*/
#include "bucketoptionswidget.h"
#include "ui_bucketoptionswidget.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QWidget>
#include <QColor>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QSpacerItem>
#include <QApplication>
#include <QCursor>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QEvent>

#include "spinslider.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "pencilsettings.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include "../CUI/cosmicbutton.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmicignorecolor.h"
#include "cosmicsplittoollayout.h"

#include "layer.h"
#include "editor.h"
#include "layermanager.h"
#include "toolmanager.h"
#include "colormanager.h"
#include "eyedroppertool.h"
#include "util.h"

namespace {

} // namespace

BucketOptionsWidget::BucketOptionsWidget(Editor* editor, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::BucketOptionsWidget),
    mEditor(editor)
{
    ui->setupUi(this);

    // Allow the widget to grow to fit its dynamically-built cosmic panel.
    if (layout()) {
        layout()->setSizeConstraint(QLayout::SetMinimumSize);
    }

    // Replace stroke thickness QDoubleSpinBox with CosmicInputField (unified numeric template)
    if (!mStrokeThicknessField && ui->strokeThicknessSpinBox) {
        QDoubleSpinBox* oldSpin = ui->strokeThicknessSpinBox;
        QHBoxLayout* hbox = ui->horizontalLayout;
        if (hbox) {
            mStrokeThicknessField = new CosmicInputField(oldSpin->parentWidget());
            mStrokeThicknessField->setObjectName("cosmicStrokeThicknessField");
            mStrokeThicknessField->setVariant(CosmicInputField::Minimal);
            mStrokeThicknessField->setDecimals(oldSpin->decimals());
            mStrokeThicknessField->setSingleStep(oldSpin->singleStep());
            mStrokeThicknessField->setRange(oldSpin->minimum(), oldSpin->maximum());
            mStrokeThicknessField->setSuffix(oldSpin->suffix());
            mStrokeThicknessField->setFixedHeight(24);
            mStrokeThicknessField->setToolTip(oldSpin->toolTip());
            mStrokeThicknessField->setValue(oldSpin->value());

            const int idx = hbox->indexOf(oldSpin);
            hbox->removeWidget(oldSpin);
            oldSpin->hide();
            if (idx >= 0) {
                hbox->insertWidget(idx, mStrokeThicknessField);
            } else {
                hbox->addWidget(mStrokeThicknessField);
            }

            connect(mStrokeThicknessField, &CosmicInputField::valueCommitted, this, [this](double v) {
                if (!mEditor || !mEditor->tools())
                    return;
                auto tm = mEditor->tools();
                tm->beginEditingPropertiesForTool(tm->currentTool()->type());
                tm->setWidth(v);
                tm->endEditingProperties();
            });
        }
    }

    ui->colorToleranceSlider->init(tr("Color tolerance"), SpinSlider::GROWTH_TYPE::LINEAR, SpinSlider::VALUE_TYPE::INTEGER, 0, MAX_COLOR_TOLERANCE);
    ui->strokeThicknessSlider->init(tr("Stroke thickness"), SpinSlider::GROWTH_TYPE::LOG, SpinSlider::VALUE_TYPE::FLOAT, 1, MAX_STROKE_THICKNESS);
    ui->colorToleranceSlider->setSliderColor(BlenderStyleSlider::getAccentColor());
    ui->strokeThicknessSlider->setSliderColor(BlenderStyleSlider::getAccentColor());

    QSettings settings(PENCIL2D, PENCIL2D);

    ui->colorToleranceCheckbox->setChecked(false);
    // Expand Fill UI removed

    // Expand Fill UI removed
    ui->strokeThicknessSpinBox->setMaximum(MAX_STROKE_THICKNESS);
    ui->colorToleranceSpinbox->setMaximum(MAX_COLOR_TOLERANCE);
    ui->strokeThicknessSpinBox->setMinimum(1);

    ui->referenceLayerComboBox->addItem(tr("Current", "Reference Layer Options"), 0);
    ui->referenceLayerComboBox->addItem(tr("All", "Reference Layer Options"), 1);
    ui->referenceLayerComboBox->setToolTip(tr("Refers to the layer that used to flood fill from"));

    ui->blendModeComboBox->addItem(tr("Overlay", "Blend Mode dropdown option"), 0);
    ui->blendModeComboBox->addItem(tr("Replace", "Blend Mode dropdown option"), 1);
    ui->blendModeComboBox->addItem(tr("Behind",  "Blend Mode dropdown option"), 2);
    ui->blendModeComboBox->setToolTip(tr("Defines how the fill will behave when the new color is not opaque"));

    // Color Replace toggle (Cosmic checkbox, textless; label lives in split layout)
    if (!mColorReplaceToggle) {
        mColorReplaceToggle = new CosmicCheckboxToggle(this);
        mColorReplaceToggle->setObjectName("colorReplaceToggle");
        mColorReplaceToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        mColorReplaceToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
        mColorReplaceToggle->setShowText(false);
        mColorReplaceToggle->setShowIcon(true);
        mColorReplaceToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        mColorReplaceToggle->setToolTip(tr("Replace all matching pixels with the current color"));
        mColorReplaceToggle->setChecked(settings.value("ColorReplaceMode", false).toBool());
        connect(mColorReplaceToggle, &QAbstractButton::toggled, this, [this](bool on){
            mEditor->tools()->setColorReplaceMode(on);
        });
    }
    if (auto rep = findChild<QCheckBox*>("colorReplaceCheckbox")) {
        rep->setToolTip(tr("Replace all matching pixels with the current color"));
        rep->setChecked(settings.value("ColorReplaceMode", false).toBool());
        rep->hide();
    }

    // Same Color toggle: restricts drag-to-fill to initial clicked color (Cosmic checkbox)
    if (!mSameColorToggle) {
        mSameColorToggle = new CosmicCheckboxToggle(this);
        mSameColorToggle->setObjectName("bucketSameColorToggle");
        mSameColorToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        mSameColorToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
        mSameColorToggle->setShowText(false);
        mSameColorToggle->setShowIcon(true);
        mSameColorToggle->setToolTip(tr("Restrict fill while dragging to the initially clicked color"));
        mSameColorToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        mSameColorToggle->setChecked(settings.value("BucketSameColorOnly", false).toBool());
        connect(mSameColorToggle, &QAbstractButton::toggled, this, [this](bool on){
            mEditor->tools()->setBucketSameColorOnly(on);
        });
    }
    if (auto sameColor = findChild<QCheckBox*>("bucketSameColorCheckbox")) {
        sameColor->setChecked(settings.value("BucketSameColorOnly", false).toBool());
        sameColor->hide();
    }

    // Disable tolerance UI and behavior entirely
    ui->colorToleranceCheckbox->hide();
    ui->colorToleranceSlider->hide();
    ui->colorToleranceSpinbox->hide();

    // Expand Fill UI removed

    connect(ui->strokeThicknessSlider, &SpinSlider::sliderPressed, this, [this]() {
        mEditor->tools()->beginEditingPropertiesForTool(mEditor->tools()->currentTool()->type());
    });
    connect(ui->strokeThicknessSlider, &SpinSlider::valueChanged, mEditor->tools(), &ToolManager::setWidth);
    connect(ui->strokeThicknessSlider, &SpinSlider::sliderReleased, mEditor->tools(), &ToolManager::endEditingProperties);
    if (!mStrokeThicknessField) {
        connect(ui->strokeThicknessSpinBox, &QDoubleSpinBox::editingFinished, this, [this]() {
            // Persist width change immediately when user finishes editing the spinbox.
            // Begin/end the property editing transaction so persistence semantics match the slider.
            auto tm = mEditor->tools();
            tm->beginEditingPropertiesForTool(tm->currentTool()->type());
            tm->setWidth(ui->strokeThicknessSpinBox->value());
            tm->endEditingProperties();
        });
    }

    connect(mEditor->tools(), &ToolManager::toolPropertyChanged, this, &BucketOptionsWidget::onPropertyChanged);
    connect(mEditor->layers(), &LayerManager::currentLayerChanged, this, &BucketOptionsWidget::onLayerChanged);

    // Create cosmic dropdown for Reference Layer (control only; layout handled by CosmicSplitToolLayout)
    if (!mReferenceLayerDropDown && ui->referenceLayerComboBox) {
        mReferenceLayerDropDown = new CosmicDropDown(this);
        mReferenceLayerDropDown->setOptions({tr("Current"), tr("All")});
        mReferenceLayerDropDown->setCurrentIndex(settings.value(SETTING_BUCKET_FILL_REFERENCE_MODE, 0).toInt());
        connect(mReferenceLayerDropDown, &CosmicDropDown::currentIndexChanged, mEditor->tools(), &ToolManager::setBucketFillReferenceMode);
        ui->referenceLayerComboBox->hide();
        ui->referenceLayerDescLabel->hide();
    }

    // Create cosmic dropdown for Gap Fill strength (Off/Low/Medium/High);
    // control exists for settings compatibility, but UI will be hidden for bitmap bucket.
    if (!mGapFillDropDown) {
        mGapFillDropDown = new CosmicDropDown(this);
        mGapFillDropDown->setOptions({tr("Gap Fill Off"), tr("Gap Fill Low"), tr("Gap Fill Medium"), tr("Gap Fill High")});
        int savedGapMode = settings.value(SETTING_BUCKET_GAP_FILL_MODE, 0).toInt();
        mGapFillDropDown->setCurrentIndex(savedGapMode);
        connect(mGapFillDropDown, &CosmicDropDown::currentIndexChanged, this, [this](int idx){
            mEditor->tools()->setBucketGapFillMode(idx);
        });
        ui->gridLayout->setRowMinimumHeight(ui->gridLayout->rowCount(), 0);
    }

    // Create cosmic dropdown for Blend Mode; control kept for compatibility but hidden for bitmap bucket
    if (!mBlendModeDropDown && ui->blendModeComboBox) {
        mBlendModeDropDown = new CosmicDropDown(this);
        mBlendModeDropDown->setOptions({tr("Overlay"), tr("Replace"), tr("Behind")});
        mBlendModeDropDown->setCurrentIndex(settings.value(SETTING_FILL_MODE, 1).toInt());
        connect(mBlendModeDropDown, &CosmicDropDown::currentIndexChanged, mEditor->tools(), &ToolManager::setFillMode);
        ui->blendModeComboBox->hide();
        ui->blendModeLabel->hide();
    }

    // Expand Fill UI removed
    ui->colorToleranceSlider->setValue(0);
    ui->colorToleranceSpinbox->setValue(0);
    // Initial selection moved to Blender-style dropdowns above

    clearFocusOnFinished(ui->colorToleranceSpinbox);
    // Expand Fill UI removed

    // Multi-Keyframe Fill toggle (Cosmic checkbox)
    if (!mMultiKeyframeFillToggle) {
        mMultiKeyframeFillToggle = new CosmicCheckboxToggle(this);
        mMultiKeyframeFillToggle->setObjectName("bucketMultiKeyframeToggle");
        mMultiKeyframeFillToggle->setMode(CosmicCheckboxToggle::DefaultCheckbox);
        mMultiKeyframeFillToggle->setScaleMode(CosmicCheckboxToggle::ScaleBig);
        mMultiKeyframeFillToggle->setShowText(false);
        mMultiKeyframeFillToggle->setShowIcon(true);
        mMultiKeyframeFillToggle->setToolTip(tr("Fill across a frame range using a movable gizmo"));
        mMultiKeyframeFillToggle->setAccentColor(BlenderStyleSlider::getAccentColor());
        mMultiKeyframeFillToggle->setChecked(settings.value("BucketMultiKeyframeFill", false).toBool());
        connect(mMultiKeyframeFillToggle, &QAbstractButton::toggled, this, [this](bool on){
            mEditor->tools()->setBucketMultiKeyframeFill(on);
        });
    }
    if (auto multiKF = findChild<QCheckBox*>("bucketMultiKeyframeFillCheckbox")) {
        multiKF->setChecked(settings.value("BucketMultiKeyframeFill", false).toBool());
        multiKF->hide();
    }

    // Build bitmap bucket options panel using CosmicSplitToolLayout
    if (!mCosmicBucketPanel) {
        if (auto* rootLayout = qobject_cast<QVBoxLayout*>(this->layout())) {
            QWidget* panel = new QWidget(this);
            panel->setObjectName("cosmicBucketPanel");
            auto* panelLayout = new QVBoxLayout(panel);
            panelLayout->setContentsMargins(0, 0, 0, 0);
            panelLayout->setSpacing(4);

            QWidget* rowsArea = new QWidget(panel);
            rowsArea->setObjectName("cosmicBucketRowsArea");
            auto* rowsLayout = new QVBoxLayout(rowsArea);
            rowsLayout->setContentsMargins(0, 0, 0, 0);
            rowsLayout->setSpacing(8);
            panelLayout->addWidget(rowsArea);

            CosmicSplitToolLayout splitLayout(rowsLayout, rowsArea);

            // Layer reference dropdown
            if (mReferenceLayerDropDown) {
                mLabelLayer = splitLayout.createEntry(tr("Layer"), mReferenceLayerDropDown);
            }

            // Replace Color toggle
            if (mColorReplaceToggle) {
                mLabelReplaceColor = splitLayout.createEntry(tr("Replace Color"), mColorReplaceToggle);
            }

            // Same Color toggle
            if (mSameColorToggle) {
                mLabelSameColor = splitLayout.createEntry(tr("Same Color"), mSameColorToggle);
            }

            // Multi-Keyframe toggle
            if (mMultiKeyframeFillToggle) {
                mLabelMultiKF = splitLayout.createEntry(tr("Multi-Keyframe"), mMultiKeyframeFillToggle);
            }

            // Ignore color row: composite CosmicIgnoreColor control
            if (!mIgnoreColorWidget) {
                mIgnoreColorWidget = new CosmicIgnoreColor(panel);
                mIgnoreColorWidget->setObjectName("bucketIgnoreColorWidget");

                // Sync enable state with editor
                connect(mIgnoreColorWidget, &CosmicIgnoreColor::ignoreEnabledChanged,
                        this, &BucketOptionsWidget::onIgnoreInFillToggled);

                // Toggling the color swatch checkbox enters/exits sample-only eyedropper mode.
                connect(mIgnoreColorWidget, &CosmicIgnoreColor::colorPickToggled,
                        this, [this](bool on){
                    if (on) {
                        onIgnoreColorClicked();
                    } else {
                        // If picker is cancelled, make sure temp tool is cleared.
                        mEditor->tools()->endTemporaryTool();
                    }
                });
            }

            mLabelIgnoreColor = splitLayout.createEntry(tr("Ignore color"), mIgnoreColorWidget);

            // Insert panel at the top of the bucket options
            rootLayout->insertWidget(0, panel);
            mCosmicBucketPanel = panel;
        }
    }

    updatePropertyVisibility();

    // Hide Expand Fill UI unconditionally
    if (ui->expandCheckbox) ui->expandCheckbox->hide();
    if (ui->expandSlider) ui->expandSlider->hide();
    if (ui->expandSpinBox) ui->expandSpinBox->hide();

    // Build ignore in fill behavior wiring for the new cosmic controls
    buildAutoFillUI();
}

BucketOptionsWidget::~BucketOptionsWidget()
{
    delete ui;
}

// ---- Existing property helpers ----
void BucketOptionsWidget::updatePropertyVisibility()
{
    Layer* layer = mEditor->layers()->currentLayer();
    Q_ASSERT(layer != nullptr);
    switch (layer->type()) {
    case Layer::VECTOR:
        ui->strokeThicknessSlider->show();
        if (mStrokeThicknessField) {
            mStrokeThicknessField->show();
        } else {
            ui->strokeThicknessSpinBox->show();
        }

        ui->colorToleranceCheckbox->hide();
        ui->colorToleranceSlider->hide();
        ui->colorToleranceSpinbox->hide();
        ui->expandCheckbox->hide();
        ui->expandSlider->hide();
        ui->expandSpinBox->hide();
        if (mReferenceLayerDropDown) mReferenceLayerDropDown->hide();
        ui->referenceLayerDescLabel->hide();
        if (mBlendModeDropDown) mBlendModeDropDown->hide();
        ui->blendModeLabel->hide();
        if (mGapFillDropDown) mGapFillDropDown->hide();
        if (mCosmicBucketPanel) mCosmicBucketPanel->hide();
        break;
    case Layer::BITMAP:
        ui->strokeThicknessSlider->hide();
        if (mStrokeThicknessField) {
            mStrokeThicknessField->hide();
        } else {
            ui->strokeThicknessSpinBox->hide();
        }

        // Bitmap bucket uses the cosmic split panel for all options.
        if (mCosmicBucketPanel) mCosmicBucketPanel->show();

        ui->colorToleranceCheckbox->hide();
        ui->colorToleranceSlider->hide();
        ui->colorToleranceSpinbox->hide();
        // Expand Fill UI removed; keep hidden
        ui->expandCheckbox->hide();
        ui->expandSlider->hide();
        ui->expandSpinBox->hide();
        // Legacy reference/blend/gap widgets stay hidden; options are surfaced via cosmic panel only.
        if (mReferenceLayerDropDown) mReferenceLayerDropDown->show();
        ui->referenceLayerDescLabel->hide();
        if (mBlendModeDropDown) mBlendModeDropDown->hide();
        ui->blendModeLabel->hide();
        if (mGapFillDropDown) mGapFillDropDown->hide();
        break;
    default:
        ui->strokeThicknessSlider->hide();
        if (mStrokeThicknessField) {
            mStrokeThicknessField->hide();
        } else {
            ui->strokeThicknessSpinBox->hide();
        }
        ui->colorToleranceCheckbox->hide();
        ui->colorToleranceSlider->hide();
        ui->colorToleranceSpinbox->hide();
        ui->expandCheckbox->hide();
        ui->expandSlider->hide();
        ui->expandSpinBox->hide();
        if (mReferenceLayerDropDown) mReferenceLayerDropDown->hide();
        ui->referenceLayerDescLabel->hide();
        if (mBlendModeDropDown) mBlendModeDropDown->hide();
        ui->blendModeLabel->hide();
        if (mGapFillDropDown) mGapFillDropDown->hide();
        if (mCosmicBucketPanel) mCosmicBucketPanel->hide();
    }
}

void BucketOptionsWidget::onPropertyChanged(ToolType, ToolPropertyType propertyType)
{
    const Properties& p = mEditor->tools()->currentTool()->properties;
    switch (propertyType)
    {
    case ToolPropertyType::TOLERANCE: setColorTolerance(static_cast<int>(p.tolerance)); break;
    case ToolPropertyType::USETOLERANCE: setColorToleranceEnabled(p.toleranceEnabled); break;
    case ToolPropertyType::WIDTH: setStrokeWidth(static_cast<int>(p.width)); break;
    case ToolPropertyType::BUCKETFILLEXPAND: setFillExpand(static_cast<int>(p.bucketFillExpand)); break;
    case ToolPropertyType::USEBUCKETFILLEXPAND: setFillExpandEnabled(p.bucketFillExpandEnabled); break;
    case ToolPropertyType::BUCKETFILLLAYERREFERENCEMODE: setFillReferenceMode(p.bucketFillReferenceMode); break;
    case ToolPropertyType::FILL_MODE: setFillMode(p.fillMode); {
            if (auto cb = findChild<QCheckBox*>("colorReplaceCheckbox")) {
                QSignalBlocker b(cb);
                cb->setChecked(p.colorReplaceMode);
            }
            if (auto sc = findChild<QCheckBox*>("bucketSameColorCheckbox")) {
                QSignalBlocker b2(sc);
                sc->setChecked(p.bucketSameColorOnly);
            }
            if (mColorReplaceToggle) {
                QSignalBlocker b3(mColorReplaceToggle);
                mColorReplaceToggle->setChecked(p.colorReplaceMode);
            }
            if (mSameColorToggle) {
                QSignalBlocker b4(mSameColorToggle);
                mSameColorToggle->setChecked(p.bucketSameColorOnly);
            }
        }
        break;
    case ToolPropertyType::BUCKET_GAP_FILL_MODE:
        setGapFillMode(p.bucketGapFillMode);
        break;
    default: break;
    }
}

void BucketOptionsWidget::onLayerChanged(int)
{
    updatePropertyVisibility();
}

void BucketOptionsWidget::setColorTolerance(int tolerance)
{
    QSignalBlocker b(ui->colorToleranceSlider);
    ui->colorToleranceSlider->setValue(tolerance);
    QSignalBlocker b2(ui->colorToleranceSpinbox);
    ui->colorToleranceSpinbox->setValue(tolerance);
}

void BucketOptionsWidget::setColorToleranceEnabled(bool enabled)
{
    QSignalBlocker b(ui->colorToleranceCheckbox);
    ui->colorToleranceCheckbox->setChecked(enabled);
}

void BucketOptionsWidget::setFillMode(int mode)
{
    if (mBlendModeDropDown) {
        QSignalBlocker b(mBlendModeDropDown);
        mBlendModeDropDown->setCurrentIndex(mode);
    }
}

void BucketOptionsWidget::setFillExpandEnabled(bool enabled)
{
    Q_UNUSED(enabled);
}

void BucketOptionsWidget::setFillExpand(int value)
{
    Q_UNUSED(value);
}

void BucketOptionsWidget::setFillReferenceMode(int referenceMode)
{
    if (mReferenceLayerDropDown) {
        QSignalBlocker b(mReferenceLayerDropDown);
        mReferenceLayerDropDown->setCurrentIndex(referenceMode);
    }
}

void BucketOptionsWidget::setGapFillMode(int mode)
{
    if (mGapFillDropDown) {
        QSignalBlocker b(mGapFillDropDown);
        mGapFillDropDown->setCurrentIndex(mode);
    }
}

void BucketOptionsWidget::setStrokeWidth(qreal value)
{
    QSignalBlocker b(ui->strokeThicknessSlider);
    ui->strokeThicknessSlider->setValue(value);
    if (mStrokeThicknessField) {
        QSignalBlocker b2(mStrokeThicknessField);
        mStrokeThicknessField->setValue(value);
    } else {
        QSignalBlocker b2(ui->strokeThicknessSpinBox);
        ui->strokeThicknessSpinBox->setValue(value);
    }
}



void BucketOptionsWidget::updateUI()
{
    BaseTool* tool = mEditor->tools()->currentTool();
    if (!tool || tool->type() != BUCKET) return;

    const Properties& p = tool->properties;

    // Sync numeric values
    setStrokeWidth(p.width);

    // Sync BlenderStyle Widgets
    if (mReferenceLayerDropDown) {
        QSignalBlocker b(mReferenceLayerDropDown);
        mReferenceLayerDropDown->setCurrentIndex(p.bucketFillReferenceMode);
    }

    if (mBlendModeDropDown) {
        QSignalBlocker b(mBlendModeDropDown);
        mBlendModeDropDown->setCurrentIndex(p.fillMode);
    }

    if (mGapFillDropDown) {
        QSignalBlocker b(mGapFillDropDown);
        mGapFillDropDown->setCurrentIndex(p.bucketGapFillMode);
    }

    if (mColorReplaceToggle) {
        QSignalBlocker b(mColorReplaceToggle);
        mColorReplaceToggle->setChecked(p.colorReplaceMode);
    }

    if (mSameColorToggle) {
        QSignalBlocker b(mSameColorToggle);
        mSameColorToggle->setChecked(p.bucketSameColorOnly);
    }

    if (mMultiKeyframeFillToggle) {
        QSignalBlocker b(mMultiKeyframeFillToggle);
        mMultiKeyframeFillToggle->setChecked(p.bucketMultiKeyframeFill);
    }

    // Update layer-specific visibility
    updatePropertyVisibility();
}

// ---- Auto-line fill helpers ----

void BucketOptionsWidget::buildAutoFillUI()
{
    // Initialize editor-side ignore-in-fill state; visual UI is provided by cosmic controls.
    mIgnoreColor = qPremultiply(qRgba(0, 0, 0, 255));
    QColor visible = QColor::fromRgba(qUnpremultiply(mIgnoreColor));
    if (!visible.isValid()) visible = Qt::black;
    if (mIgnoreColorWidget) {
        mIgnoreColorWidget->setIgnoreColor(visible);
        mIgnoreColorWidget->setIgnoreEnabled(true);
        mIgnoreColorWidget->setColorPickerActive(false);
    }

    mEditor->setIgnoreFillColor(mIgnoreColor);
    mIgnoreInFillEnabled = true;
    mEditor->setIgnoreFillEnabled(true);

    // Keep this UI in sync when other panels change ignore-in-fill
    connect(mEditor, &Editor::ignoreFillColorChanged, this, [this](QRgb color){
        mIgnoreColor = color;
        QColor c = QColor::fromRgba(qUnpremultiply(color));
        if (!c.isValid()) c = Qt::black;
        if (mIgnoreColorWidget) {
            mIgnoreColorWidget->setIgnoreColor(c);
        }
    });
    connect(mEditor, &Editor::ignoreFillEnabledChanged, this, [this](bool on){
        mIgnoreInFillEnabled = on;
        if (mIgnoreColorWidget) {
            mIgnoreColorWidget->setIgnoreEnabled(on);
        }
    });
}

void BucketOptionsWidget::onIgnoreInFillToggled(bool checked)
{
    mIgnoreInFillEnabled = checked;
    if (mEditor) {
        mEditor->setIgnoreFillEnabled(checked);
    }
}

// Auto-line master toggle and color toggles moved to canvas header area
// syncAutoFillState, onAutoLineMasterToggled, and onColorCheckToggled no longer needed

void BucketOptionsWidget::onIgnoreColorClicked()
{
    // Clean up any previous attempt to avoid getting stuck in temp tool or override cursor
    mEditor->tools()->endTemporaryTool();
    while (QApplication::overrideCursor()) { QApplication::restoreOverrideCursor(); }
    this->unsetCursor();
    if (auto* pw = this->parentWidget()) pw->unsetCursor();
    // Do not broad-disconnect ColorManager signals; per-attempt connections are self-disconnecting

    // Use the public temporary tool method: sample-only so it doesn't change current color
    mEditor->tools()->setEyedropperSampleOnly(true);
    bool success = mEditor->tools()->beginTemporaryTool(EYEDROPPER, Qt::Key_unknown);
    
    if (success) {
        mEyedropperMode = true;
        // Show eyedropper cursor within the options area too
        QCursor dropper(QPixmap(":/icons/general/cursor-eyedropper.svg"), 0, 15);
        this->setCursor(dropper);
        if (auto* w = this->parentWidget()) w->setCursor(dropper);
        QApplication::setOverrideCursor(dropper);
        // Connect directly to eyedropper sample signal
        if (auto* dropper = qobject_cast<EyedropperTool*>(mEditor->tools()->getTool(EYEDROPPER))) {
            auto colorConn = std::make_shared<QMetaObject::Connection>();
            *colorConn = connect(dropper, &EyedropperTool::sampledColor, this, [this, colorConn](const QColor& picked){
                // Apply to ignore-fill without changing current color
                if (!picked.isValid()) { disconnect(*colorConn); return; }
                mIgnoreColor = qPremultiply(picked.rgba());
                if (mIgnoreColorWidget) {
                    QColor c = picked.isValid() ? picked : Qt::black;
                    mIgnoreColorWidget->setIgnoreColor(c);
                    mIgnoreColorWidget->setColorPickerActive(false);
                }
                mEditor->setIgnoreFillColor(mIgnoreColor);
                disconnect(*colorConn);
            }, Qt::UniqueConnection);
        }
        // Also restore cursor if the tool changes without a pick
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(mEditor->tools(), &ToolManager::toolChanged, this, [this, conn](ToolType){
            const bool wasActive = mEyedropperMode;
            mEyedropperMode = false;
            if (wasActive) {
                this->unsetCursor();
                if (auto* w = this->parentWidget()) w->unsetCursor();
                QApplication::restoreOverrideCursor();
            }
            mEditor->tools()->setEyedropperSampleOnly(false);
            disconnect(*conn);
        }, Qt::UniqueConnection);
    } else {
        mEyedropperMode = false;
    }
}

void BucketOptionsWidget::onEyedropperColorPicked()
{
    if (!mEyedropperMode) return;
    
    mEyedropperMode = false;
    
    // Disconnect the signal
    disconnect(mEditor->color(), &ColorManager::colorChanged, this, &BucketOptionsWidget::onEyedropperColorPicked);
    
    // Get the picked color (captured temporarily via frontColor)
    QColor pickedColor = mEditor->color()->frontColor();
    if (!pickedColor.isValid()) return;
    
    mIgnoreColor = qPremultiply(pickedColor.rgba());
    if (mIgnoreColorWidget) {
        QColor c = pickedColor.isValid() ? pickedColor : Qt::black;
        mIgnoreColorWidget->setIgnoreColor(c);
        mIgnoreColorWidget->setColorPickerActive(false);
    }
    mEditor->setIgnoreFillColor(mIgnoreColor);
    
    // Defer ending the temp tool and restoring cursors until after the release event completes
    QTimer::singleShot(0, this, [this]() {
        mEditor->tools()->endTemporaryTool();
        this->unsetCursor();
        if (auto* w = this->parentWidget()) w->unsetCursor();
        QApplication::restoreOverrideCursor();
    });
}

void BucketOptionsWidget::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        auto safetr = [this](const char* src) -> QString {
            const QString t = tr(src);
            return t.isEmpty() ? QString::fromUtf8(src) : t;
        };

        ui->retranslateUi(this);

        // Dropdown options
        if (mReferenceLayerDropDown) {
            const int idx = mReferenceLayerDropDown->currentIndex();
            QSignalBlocker b(mReferenceLayerDropDown);
            mReferenceLayerDropDown->setOptions({ safetr("Current"), safetr("All") });
            mReferenceLayerDropDown->setCurrentIndex(idx);
        }
        if (mBlendModeDropDown) {
            const int idx = mBlendModeDropDown->currentIndex();
            mBlendModeDropDown->setOptions({ safetr("Overlay"), safetr("Replace"), safetr("Behind") });
            mBlendModeDropDown->setCurrentIndex(idx);
        }
        if (mGapFillDropDown) {
            const int idx = mGapFillDropDown->currentIndex();
            mGapFillDropDown->setOptions({ safetr("Gap Fill Off"), safetr("Gap Fill Low"), safetr("Gap Fill Medium"), safetr("Gap Fill High") });
            mGapFillDropDown->setCurrentIndex(idx);
        }

        // Row labels
        auto trLbl = [this](QLabel* lbl, const char* src) {
            if (!lbl) return;
            const QString t = tr(src);
            if (!t.isEmpty()) lbl->setText(t);
        };
        trLbl(mLabelLayer,        "Layer");
        trLbl(mLabelReplaceColor, "Replace Color");
        trLbl(mLabelSameColor,    "Same Color");
        trLbl(mLabelMultiKF,      "Multi-Keyframe");
        trLbl(mLabelIgnoreColor,  "Ignore color");
    }
}

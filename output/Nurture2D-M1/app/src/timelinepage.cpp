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

#include "timelinepage.h"

#include "pencildef.h"
#include <QScrollBar>
#include <QEvent>
#include "preferencemanager.h"

#include "ui_timelinepage.h"
#include "../paintlyFillAlgorithm/blenderstyleslider.h"
#include "../CUI/blenderstylevaluefield.h"
#include "../CUI/cosmicdropdown.h"
#include "../CUI/cosmicuimetrics.h"
#include "../CUI/cosmicinputfield.h"
#include "../CUI/cosmiccheckboxtoggle.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>

TimelinePage::TimelinePage()
    : ui(new Ui::TimelinePage)
{
    ui->setupUi(this);

    if (ui->scrollArea) {
        ui->scrollArea->setFrameShape(QFrame::NoFrame);
        ui->scrollArea->setWidgetResizable(true);
        ui->scrollArea->viewport()->setAutoFillBackground(false);
        ui->scrollArea->setAutoFillBackground(false);
    }

    {
        const int cr = qMax(0, CosmicUiMetrics::cornerRadius());
        const QString cardStyle = QString(
            "QScrollArea{background:transparent;border:none;}"
            "QWidget#scrollAreaWidgetContents{background:transparent;}"
            "QLabel{background:transparent;}"
            "QCheckBox{background:transparent;}"
            "QGroupBox{"
            "  background:#2b2b2b;"
            "  border:none;"
            "  border-radius:%1px;"
            "  margin-top:10px;"
            "  padding:12px;"
            "}"
            "QGroupBox::title{height:0px; padding:0px; margin:0px; border:none; color:transparent;}"
            "QLabel#cosmicPrefSectionHeader{"
            " background:transparent;"
            " color:#b3b3b3;"
            " font-weight:600;"
            " padding:0px;"
            " margin:0px;"
            "}"
        ).arg(QString::number(cr));
        setStyleSheet(styleSheet() + cardStyle);
    }

    if (ui->verticalLayout_4) {
        ui->verticalLayout_4->setContentsMargins(12, 12, 12, 12);
        ui->verticalLayout_4->setSpacing(10);
    }

    if (ui->timeLineBox) { ui->timeLineBox->setTitle(QString()); ui->timeLineBox->setFlat(true); }
    if (ui->emptyFrameBox) { ui->emptyFrameBox->setTitle(QString()); ui->emptyFrameBox->setFlat(true); }
    if (ui->flipRollBox) { ui->flipRollBox->setTitle(QString()); ui->flipRollBox->setFlat(true); }
    if (ui->soundScrubBox) { ui->soundScrubBox->setTitle(QString()); ui->soundScrubBox->setFlat(true); }
    if (ui->layerVisibilityBox) { ui->layerVisibilityBox->setTitle(QString()); ui->layerVisibilityBox->setFlat(true); }

    auto insertHeader = [](QGroupBox* gb, const QString& title) -> QLabel* {
        if (!gb) return nullptr;
        QLayout* layout = gb->layout();
        if (!layout) return nullptr;
        auto* header = new QLabel(title, gb);
        header->setObjectName(QStringLiteral("cosmicPrefSectionHeader"));
        if (auto* vbox = qobject_cast<QVBoxLayout*>(layout)) {
            vbox->insertWidget(0, header);
        }
        return header;
    };
    mHeaderTimeline   = insertHeader(ui->timeLineBox,       tr("Timeline"));
    mHeaderDrawing    = insertHeader(ui->emptyFrameBox,     tr("Drawing"));
    mHeaderFlipRoll   = insertHeader(ui->flipRollBox,       tr("Flip and Roll"));
    mHeaderSoundScrub = insertHeader(ui->soundScrubBox,     tr("Sound scrub"));
    mHeaderLayerVis   = insertHeader(ui->layerVisibilityBox, tr("Layer Visibility"));
    
    ui->scrollArea->verticalScrollBar()->setFixedWidth(12);
    ui->scrollArea->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical {"
        "  background: #1f1f1f;"
        "  border: none;"
        "  border-radius: 6px;"
        "  width: 12px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #505050;"
        "  border-radius: 6px;"
        "  min-height: 26px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { background: transparent; height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    {
        QSpinBox* oldTimelineLength = ui->timelineLength;
        
        // Find the actual HBox layout (not the VBoxLayout of the parent widget)
        QHBoxLayout* hbox = nullptr;
        QWidget* parent = oldTimelineLength->parentWidget();
        if (parent && parent->layout()) {
            // The parent is the GroupBox with a VBoxLayout
            // We need to find the HBoxLayout inside the VBoxLayout
            QVBoxLayout* vbox = qobject_cast<QVBoxLayout*>(parent->layout());
            if (vbox && vbox->count() > 0) {
                QLayoutItem* item = vbox->itemAt(0);
                if (item) {
                    hbox = qobject_cast<QHBoxLayout*>(item->layout());
                }
            }
        }
        
        if (!hbox) {
            qCritical() << "Could not find HBox layout for timelineLength!";
        } else {
            auto* timelineField = new CosmicInputField(parent);
            timelineField->setRange(1.0, MaxFramesBound);
            timelineField->setDecimals(0);
            timelineField->setSingleStep(1.0);
            timelineField->setValue(oldTimelineLength->value());

            // Keep the label visible; only replace the spinbox in the row.
            int index = hbox->indexOf(oldTimelineLength);
            if (index >= 0) {
                hbox->removeWidget(oldTimelineLength);
                oldTimelineLength->hide();
                hbox->insertWidget(index, timelineField, 0);
            } else {
                oldTimelineLength->hide();
                hbox->addWidget(timelineField, 0);
            }

            mTimelineLengthField = timelineField;
        }
    }

    {
        QSpinBox* oldSpinBox = ui->flipRollNumDrawingsSpinBox;
        QSlider* oldSlider = ui->flipRollNumDrawingsSlider;
        
        // Find the actual grid layout (not the VBoxLayout of the parent widget)
        QGridLayout* grid = nullptr;
        QWidget* parent = oldSlider->parentWidget();
        if (parent && parent->layout()) {
            // The parent is the GroupBox with a VBoxLayout
            // We need to find the GridLayout inside the VBoxLayout
            QVBoxLayout* vbox = qobject_cast<QVBoxLayout*>(parent->layout());
            if (vbox && vbox->count() > 0) {
                QLayoutItem* item = vbox->itemAt(0);
                if (item) {
                    grid = qobject_cast<QGridLayout*>(item->layout());
                }
            }
        }
        
        if (!grid) {
            qCritical() << "Could not find grid layout for flipRollNumDrawings!";
        } else {
            auto* field = new CosmicInputField(parent);
            field->setRange(oldSlider->minimum(), oldSlider->maximum());
            field->setValue(oldSlider->value());
            field->setDecimals(0);
            field->setSingleStep(1.0);
            
            // Find and hide/remove the label from the grid layout
            if (QLabel* label = parent->findChild<QLabel*>("flipRollNumDrawingsLanel")) {
                grid->removeWidget(label);
                label->hide();
                label->deleteLater();
            }
            
            // Remove old widgets
            grid->removeWidget(oldSlider);
            grid->removeWidget(oldSpinBox);
            oldSlider->hide();
            oldSpinBox->hide();
            
            // Add new field at row 2, column 0, spanning 1 row and 2 columns (full width)
            grid->addWidget(field, 2, 0, 1, 2);
            grid->setColumnStretch(0, 1); // Make it expand
            
            mFlipRollDrawingsField = field;
        }
    }

    {
        QSpinBox* oldSpinBox = ui->flipRollMsecsSpinBox;
        QSlider* oldSlider = ui->flipRollMsecsSlider;
        
        // Find the actual grid layout (not the VBoxLayout of the parent widget)
        QGridLayout* grid = nullptr;
        QWidget* parent = oldSlider->parentWidget();
        if (parent && parent->layout()) {
            QVBoxLayout* vbox = qobject_cast<QVBoxLayout*>(parent->layout());
            if (vbox && vbox->count() > 0) {
                QLayoutItem* item = vbox->itemAt(0);
                if (item) {
                    grid = qobject_cast<QGridLayout*>(item->layout());
                }
            }
        }
        
        if (!grid) {
            qCritical() << "Could not find grid layout for flipRollMsecs!";
        } else {
            auto* field = new CosmicInputField(parent);
            field->setRange(oldSlider->minimum(), oldSlider->maximum());
            field->setValue(oldSlider->value());
            field->setDecimals(0);
            field->setSingleStep(1.0);
            
            // Find and hide/remove the label from the grid layout
            if (QLabel* label = parent->findChild<QLabel*>("flipRollMsecsLabel")) {
                grid->removeWidget(label);
                label->hide();
                label->deleteLater();
            }
            
            // Remove old widgets
            grid->removeWidget(oldSlider);
            grid->removeWidget(oldSpinBox);
            oldSlider->hide();
            oldSpinBox->hide();
            
            // Add new field at row 4, column 0, spanning 1 row and 2 columns (full width)
            grid->addWidget(field, 4, 0, 1, 2);
            grid->setColumnStretch(0, 1); // Make it expand
            
            mFlipRollMsecsField = field;
        }
    }

    {
        QSpinBox* oldSpinBox = ui->flipInBtwnMsecSpinBox;
        QSlider* oldSlider = ui->flipInBtwnMsecSlider;
        
        // Find the actual grid layout (not the VBoxLayout of the parent widget)
        QGridLayout* grid = nullptr;
        QWidget* parent = oldSlider->parentWidget();
        if (parent && parent->layout()) {
            QVBoxLayout* vbox = qobject_cast<QVBoxLayout*>(parent->layout());
            if (vbox && vbox->count() > 0) {
                QLayoutItem* item = vbox->itemAt(0);
                if (item) {
                    grid = qobject_cast<QGridLayout*>(item->layout());
                }
            }
        }
        
        if (!grid) {
            qCritical() << "Could not find grid layout for flipInBtwnMsec!";
        } else {
            auto* field = new CosmicInputField(parent);
            field->setRange(oldSlider->minimum(), oldSlider->maximum());
            field->setValue(oldSlider->value());
            field->setDecimals(0);
            field->setSingleStep(1.0);
            
            // Find and hide/remove the label from the grid layout
            if (QLabel* label = parent->findChild<QLabel*>("flipInBtwnMsecLabel")) {
                grid->removeWidget(label);
                label->hide();
                label->deleteLater();
            }
            
            // Remove old widgets
            grid->removeWidget(oldSlider);
            grid->removeWidget(oldSpinBox);
            oldSlider->hide();
            oldSpinBox->hide();
            
            // Add new field at row 6, column 0, spanning 1 row and 2 columns (full width)
            grid->addWidget(field, 6, 0, 1, 2);
            grid->setColumnStretch(0, 1); // Make it expand
            
            mFlipInbetweenMsecsField = field;
        }
    }

    {
        QSpinBox* oldSpinBox = ui->soundScrubSpinBox;
        QSlider* oldSlider = ui->soundScrubSlider;
        
        // Find the actual HBox layout (not the VBoxLayout of the parent widget)
        QHBoxLayout* hbox = nullptr;
        QWidget* parent = oldSlider->parentWidget();
        if (parent && parent->layout()) {
            // The parent is the GroupBox with a VBoxLayout
            // We need to find the HBoxLayout inside the VBoxLayout
            QVBoxLayout* vbox = qobject_cast<QVBoxLayout*>(parent->layout());
            if (vbox && vbox->count() > 0) {
                QLayoutItem* item = vbox->itemAt(0);
                if (item) {
                    hbox = qobject_cast<QHBoxLayout*>(item->layout());
                }
            }
        }
        
        if (!hbox) {
            qCritical() << "Could not find HBox layout for soundScrub!";
        } else {
            auto* field = new CosmicInputField(parent);
            field->setRange(oldSlider->minimum(), oldSlider->maximum());
            field->setValue(oldSlider->value());
            field->setDecimals(0);
            field->setSingleStep(1.0);
            
            hbox->removeWidget(oldSlider);
            hbox->removeWidget(oldSpinBox);
            oldSlider->hide();
            oldSpinBox->hide();
            hbox->addWidget(field, 1); // stretch factor = 1
            
            mSoundScrubMsecsField = field;
        }
    }

    {
        QSpinBox* oldSpinBox = ui->visibilitySpinbox;
        QSlider* oldSlider = ui->visibilitySlider;
        
        // Find the actual HBox layout (it's nested deeper in the VBoxLayout)
        QHBoxLayout* hbox = nullptr;
        QWidget* parent = oldSlider->parentWidget();
        if (parent && parent->layout()) {
            // The parent is the GroupBox with a VBoxLayout
            // We need to find the HBoxLayout inside the VBoxLayout
            // It's not the first item, so we need to search for it
            QVBoxLayout* vbox = qobject_cast<QVBoxLayout*>(parent->layout());
            if (vbox) {
                for (int i = 0; i < vbox->count(); ++i) {
                    QLayoutItem* item = vbox->itemAt(i);
                    if (item && item->layout()) {
                        hbox = qobject_cast<QHBoxLayout*>(item->layout());
                        if (hbox) break;
                    }
                }
            }
        }
        
        if (!hbox) {
            qCritical() << "Could not find HBox layout for visibility!";
        } else {
            auto* field = new CosmicInputField(parent);
            field->setRange(0.0, 100.0);
            field->setValue(oldSlider->value());
            field->setDecimals(0);
            field->setSingleStep(1.0);
            
            hbox->removeWidget(oldSlider);
            hbox->removeWidget(oldSpinBox);
            oldSlider->hide();
            oldSpinBox->hide();
            hbox->addWidget(field, 1); // stretch factor = 1
            
            mVisibilityThresholdField = field;
        }
    }

    if (ui->scrubBox) {
        QCheckBox* old = ui->scrubBox;
        QLayout* layout = old->parentWidget() ? old->parentWidget()->layout() : nullptr;
        if (layout) {
            auto* repl = new CosmicCheckboxToggle(old->parentWidget());
            repl->setMode(CosmicCheckboxToggle::DefaultCheckbox);
            repl->setText(old->text());
            repl->setChecked(old->isChecked());
            repl->setFixedHeight(CosmicUiMetrics::controlThickness());

            if (auto* vbox = qobject_cast<QVBoxLayout*>(layout)) {
                int idx = vbox->indexOf(old);
                vbox->removeWidget(old);
                old->hide();
                vbox->insertWidget(qMax(0, idx), repl);
            } else {
                old->hide();
                layout->addWidget(repl);
            }

            connect(old, &QCheckBox::toggled, repl, [repl](bool on) {
                QSignalBlocker b(repl);
                repl->setChecked(on);
            });
            connect(repl, &CosmicCheckboxToggle::toggled, old, [old](bool on) {
                if (old->isChecked() == on) return;
                old->setChecked(on);
            });

            mScrubToggle = repl;
        }
    }

    auto comboChanged = static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged);
    
    if (mTimelineLengthField) {
        connect(mTimelineLengthField, &CosmicInputField::valueChanged,
                this, [this](double value) { timelineLengthChanged(static_cast<int>(value)); });
    }
    connect(ui->scrubBox, &QCheckBox::stateChanged, this, &TimelinePage::scrubChanged);
    connect(ui->radioButtonAddNewKey, &QRadioButton::toggled, this, &TimelinePage::drawEmptyKeyRadioButtonToggled);
    connect(ui->radioButtonDuplicate, &QRadioButton::toggled, this, &TimelinePage::drawEmptyKeyRadioButtonToggled);
    connect(ui->radioButtonDrawOnPrev, &QRadioButton::toggled, this, &TimelinePage::drawEmptyKeyRadioButtonToggled);

    if (mFlipRollMsecsField) {
        connect(mFlipRollMsecsField, &CosmicInputField::valueChanged,
                this, [this](double value) { flipRollMsecSliderChanged(static_cast<int>(value)); });
    }
    if (mFlipRollDrawingsField) {
        connect(mFlipRollDrawingsField, &CosmicInputField::valueChanged,
                this, [this](double value) { flipRollNumDrawingsSliderChanged(static_cast<int>(value)); });
    }
    if (mFlipInbetweenMsecsField) {
        connect(mFlipInbetweenMsecsField, &CosmicInputField::valueChanged,
                this, [this](double value) { flipInbetweenMsecSliderChanged(static_cast<int>(value)); });
    }
    if (mSoundScrubMsecsField) {
        connect(mSoundScrubMsecsField, &CosmicInputField::valueChanged,
                this, [this](double value) { soundScrubMsecSliderChanged(static_cast<int>(value)); });
    }

    {
        QComboBox* old = ui->layerVisibilityComboBox;
        if (old) {
            auto* repl = new CosmicDropDown(old->parentWidget());
            repl->setOptions({ tr("Current layer only"), tr("Relative"), tr("All Layers") });

            if (QLayout* parentLayout = old->parentWidget() ? old->parentWidget()->layout() : nullptr) {
                if (auto* grid = qobject_cast<QGridLayout*>(parentLayout)) {
                    int row = -1, col = -1, rowSpan = 1, colSpan = 1;
                    int idx = grid->indexOf(old);
                    if (idx >= 0) {
                        grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
                    }
                    grid->removeWidget(old);
                    old->hide();
                    if (row >= 0 && col >= 0) {
                        grid->addWidget(repl, row, col, rowSpan, colSpan);
                    } else {
                        grid->addWidget(repl, grid->rowCount(), 0, 1, grid->columnCount() > 0 ? grid->columnCount() : 1);
                    }
                } else if (auto* hbox = qobject_cast<QHBoxLayout*>(parentLayout)) {
                    int idx = hbox->indexOf(old);
                    hbox->removeWidget(old);
                    old->hide();
                    if (idx >= 0) {
                        hbox->insertWidget(idx, repl);
                    } else {
                        hbox->addWidget(repl);
                    }
                } else if (auto* vbox = qobject_cast<QVBoxLayout*>(parentLayout)) {
                    int idx = vbox->indexOf(old);
                    vbox->removeWidget(old);
                    old->hide();
                    if (idx >= 0) {
                        vbox->insertWidget(idx, repl);
                    } else {
                        vbox->addWidget(repl);
                    }
                }
            }
            mLayerVisibilityDropDown = repl;
            connect(mLayerVisibilityDropDown, &CosmicDropDown::currentIndexChanged, this, &TimelinePage::layerVisibilityChanged);
        }
    }
    if (mVisibilityThresholdField) {
        connect(mVisibilityThresholdField, &CosmicInputField::valueChanged,
                this, [this](double value) { layerVisibilityThresholdChanged(static_cast<int>(value)); });
    }
}

TimelinePage::~TimelinePage()
{
    delete ui;
}

void TimelinePage::updateAccentColors()
{
    const QColor accent = BlenderStyleSlider::getAccentColor();
    if (mLayerVisibilityDropDown) mLayerVisibilityDropDown->setAccentColor(accent);
}

void TimelinePage::updateValues()
{
    QSignalBlocker b1(ui->scrubBox);
    ui->scrubBox->setChecked(mManager->isOn(SETTING::SHORT_SCRUB));

    if (mTimelineLengthField) {
        QSignalBlocker b3(mTimelineLengthField);
        mTimelineLengthField->setValue(mManager->getInt(SETTING::TIMELINE_SIZE));
        if (mManager->getString(SETTING::TIMELINE_SIZE).toInt() <= 0) {
            mTimelineLengthField->setValue(240);
        }
    }

    QSignalBlocker b4(ui->radioButtonAddNewKey);
    QSignalBlocker b5(ui->radioButtonDuplicate);
    QSignalBlocker b6(ui->radioButtonDrawOnPrev);
    int action = mManager->getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION);
    switch (action)
    {
    case CREATE_NEW_KEY:
        ui->radioButtonAddNewKey->setChecked(true);
        break;
    case DUPLICATE_PREVIOUS_KEY:
        ui->radioButtonDuplicate->setChecked(true);
        break;
    case KEEP_DRAWING_ON_PREVIOUS_KEY:
        ui->radioButtonDrawOnPrev->setChecked(true);
        break;
    default:
        break;
    }

    // to secure that you have a relevant minimum setting for sound scrub
    int fps = mManager->getInt(SETTING::FPS);
    int minMsec = 1000 / fps;
    if (minMsec > 100) { minMsec = 100; }
    if (mSoundScrubMsecsField) {
        QSignalBlocker b(mSoundScrubMsecsField);
        mSoundScrubMsecsField->setRange(minMsec, 400.0);
        mSoundScrubMsecsField->setValue(mManager->getInt(SETTING::SOUND_SCRUB_MSEC));
    }

    if (mFlipRollMsecsField) {
        QSignalBlocker b(mFlipRollMsecsField);
        mFlipRollMsecsField->setValue(mManager->getInt(SETTING::FLIP_ROLL_MSEC));
    }
    if (mFlipRollDrawingsField) {
        QSignalBlocker b(mFlipRollDrawingsField);
        mFlipRollDrawingsField->setValue(mManager->getInt(SETTING::FLIP_ROLL_DRAWINGS));
    }
    if (mFlipInbetweenMsecsField) {
        QSignalBlocker b(mFlipInbetweenMsecsField);
        mFlipInbetweenMsecsField->setValue(mManager->getInt(SETTING::FLIP_INBETWEEN_MSEC));
    }

    if (mVisibilityThresholdField) {
        const int convertedVisibilityThreshold = static_cast<int>(mManager->getFloat(SETTING::LAYER_VISIBILITY_THRESHOLD) * 100);
        QSignalBlocker b(mVisibilityThresholdField);
        mVisibilityThresholdField->setValue(convertedVisibilityThreshold);
    }

    int visibilityType = mManager->getInt(SETTING::LAYER_VISIBILITY);
    if (mLayerVisibilityDropDown) {
        QSignalBlocker b(mLayerVisibilityDropDown);
        mLayerVisibilityDropDown->setCurrentIndex(visibilityType);
    }
    layerVisibilityChanged(visibilityType);
}

void TimelinePage::timelineLengthChanged(int value)
{
    mManager->set(SETTING::TIMELINE_SIZE, value);
}

void TimelinePage::fontSizeChanged(int value)
{
    mManager->set(SETTING::LABEL_FONT_SIZE, value);
}

void TimelinePage::scrubChanged(int value)
{
    mManager->set(SETTING::SHORT_SCRUB, value != Qt::Unchecked);
}

void TimelinePage::layerVisibilityChanged(int value)
{
    mManager->set(SETTING::LAYER_VISIBILITY, value);
    if (mVisibilityThresholdField) {
        mVisibilityThresholdField->setEnabled(value == 1);
    }
}

void TimelinePage::layerVisibilityThresholdChanged(int value)
{
    float percentage = static_cast<float>(value/100.0f);
    mManager->set(SETTING::LAYER_VISIBILITY_THRESHOLD, percentage);
}

void TimelinePage::drawEmptyKeyRadioButtonToggled(bool)
{
    if (ui->radioButtonAddNewKey->isChecked())
    {
        mManager->set(SETTING::DRAW_ON_EMPTY_FRAME_ACTION, CREATE_NEW_KEY);
    }
    else if (ui->radioButtonDuplicate->isChecked())
    {
        mManager->set(SETTING::DRAW_ON_EMPTY_FRAME_ACTION, DUPLICATE_PREVIOUS_KEY);
    }
    else if (ui->radioButtonDrawOnPrev->isChecked())
    {
        mManager->set(SETTING::DRAW_ON_EMPTY_FRAME_ACTION, KEEP_DRAWING_ON_PREVIOUS_KEY);
    }
}

void TimelinePage::flipRollMsecSliderChanged(int value)
{
    mManager->set(SETTING::FLIP_ROLL_MSEC, value);
}

void TimelinePage::flipRollMsecSpinboxChanged(int value)
{
    // No longer needed - merged with slider
    mManager->set(SETTING::FLIP_ROLL_MSEC, value);
}

void TimelinePage::flipRollNumDrawingsSliderChanged(int value)
{
    mManager->set(SETTING::FLIP_ROLL_DRAWINGS, value);
}

void TimelinePage::flipRollNumDrawingsSpinboxChanged(int value)
{
    // No longer needed - merged with slider
    mManager->set(SETTING::FLIP_ROLL_DRAWINGS, value);
}

void TimelinePage::flipInbetweenMsecSliderChanged(int value)
{
    mManager->set(SETTING::FLIP_INBETWEEN_MSEC, value);
}

void TimelinePage::flipInbetweenMsecSpinboxChanged(int value)
{
    // No longer needed - merged with slider
    mManager->set(SETTING::FLIP_INBETWEEN_MSEC, value);
}

void TimelinePage::soundScrubActiveChanged(int i)
{
    bool b = true;
    if (i == 0)
        b = false;
    mManager->set(SETTING::SOUND_SCRUB_ACTIVE, b);
    emit soundScrubChanged(b);
}

void TimelinePage::soundScrubMsecSliderChanged(int value)
{
    mManager->set(SETTING::SOUND_SCRUB_MSEC, value);
    emit soundScrubMsecChanged(value);
}

void TimelinePage::soundScrubMsecSpinboxChanged(int value)
{
    // No longer needed - merged with slider
    mManager->set(SETTING::SOUND_SCRUB_MSEC, value);
    emit soundScrubMsecChanged(value);
}

void TimelinePage::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        retranslatePageStrings();
    }
}

void TimelinePage::retranslatePageStrings()
{
    auto safetr = [this](const char* src) -> QString {
        const QString t = tr(src);
        return t.isEmpty() ? QString::fromUtf8(src) : t;
    };

    ui->retranslateUi(this);

    // Section headers
    if (mHeaderTimeline)   mHeaderTimeline->setText(safetr("Timeline"));
    if (mHeaderDrawing)    mHeaderDrawing->setText(safetr("Drawing"));
    if (mHeaderFlipRoll)   mHeaderFlipRoll->setText(safetr("Flip and Roll"));
    if (mHeaderSoundScrub) mHeaderSoundScrub->setText(safetr("Sound scrub"));
    if (mHeaderLayerVis)   mHeaderLayerVis->setText(safetr("Layer Visibility"));

    // Layer visibility dropdown options
    if (mLayerVisibilityDropDown) {
        const int idx = mLayerVisibilityDropDown->currentIndex();
        QSignalBlocker b(mLayerVisibilityDropDown);
        mLayerVisibilityDropDown->setOptions({
            safetr("Current layer only"),
            safetr("Relative"),
            safetr("All Layers")
        });
        mLayerVisibilityDropDown->setCurrentIndex(idx);
    }

    // Scrub toggle text (mirrors hidden checkbox)
    if (mScrubToggle && ui->soundScrubBox) {
        const QString t = ui->soundScrubBox->title();
        if (!t.isEmpty()) mScrubToggle->setText(t);
    }
}

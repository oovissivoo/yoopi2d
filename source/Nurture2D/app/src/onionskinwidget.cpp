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

#include "onionskinwidget.h"
#include "ui_onionskin.h"

#include "preferencemanager.h"
#include "editor.h"
#include "flowlayout.h"
#include "util.h"
#include "../CUI/blenderstyletoggle.h"
#include "../CUI/blenderstylevaluefield.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

OnionSkinWidget::OnionSkinWidget(QWidget *parent) :
    BaseDockWidget(parent),
    ui(new Ui::OnionSkin)
{
    ui->setupUi(this);

    clearFocusOnFinished(ui->onionPrevFramesNumBox);
    clearFocusOnFinished(ui->onionNextFramesNumBox);
    clearFocusOnFinished(ui->onionMinOpacityBox);
    clearFocusOnFinished(ui->onionMaxOpacityBox);
}

OnionSkinWidget::~OnionSkinWidget()
{
    delete ui;
}

void OnionSkinWidget::initUI()
{
    // Create Blender-style toggles for Previous Frames, Next Frames, and Onion Color
    // These will be arranged horizontally with no gaps (merged appearance)
    
    // Create a horizontal layout for the three toggles
    QHBoxLayout* toggleLayout = new QHBoxLayout();
    toggleLayout->setSpacing(0); // No spacing between toggles for merged look
    toggleLayout->setContentsMargins(3, 3, 3, 3);
    
    // Previous Frames Toggle (left corners rounded)
    mPrevFramesToggle = new BlenderStyleToggle(this);
    mPrevFramesToggle->setText(tr("Previous"));
    mPrevFramesToggle->setCornerMode(BlenderStyleToggle::LeftCorners);
    mPrevFramesToggle->setBorderSides(BlenderStyleToggle::BorderLeft | BlenderStyleToggle::BorderTop | BlenderStyleToggle::BorderBottom);
    mPrevFramesToggle->setFixedHeight(28);
    toggleLayout->addWidget(mPrevFramesToggle, 1); // stretch factor 1
    
    // Next Frames Toggle (no corners rounded - middle)
    mNextFramesToggle = new BlenderStyleToggle(this);
    mNextFramesToggle->setText(tr("Next"));
    mNextFramesToggle->setCornerMode(BlenderStyleToggle::NoCorners);
    mNextFramesToggle->setBorderSides(BlenderStyleToggle::BorderTop | BlenderStyleToggle::BorderBottom);
    mNextFramesToggle->setFixedHeight(28);
    toggleLayout->addWidget(mNextFramesToggle, 1); // stretch factor 1
    
    // Onion Color Toggle (right corners rounded)
    mOnionColorToggle = new BlenderStyleToggle(this);
    mOnionColorToggle->setText(tr("Color"));
    mOnionColorToggle->setCornerMode(BlenderStyleToggle::RightCorners);
    mOnionColorToggle->setBorderSides(BlenderStyleToggle::BorderRight | BlenderStyleToggle::BorderTop | BlenderStyleToggle::BorderBottom);
    mOnionColorToggle->setFixedHeight(28);
    toggleLayout->addWidget(mOnionColorToggle, 1); // stretch factor 1
    
    // Create BlenderStyleValueField widgets for numeric controls
    mPrevFramesField = new BlenderStyleValueField(this);
    mPrevFramesField->setLabel(tr("Prev Frames"));
    mPrevFramesField->setRange(1.0, 60.0);
    mPrevFramesField->setDecimals(0);
    mPrevFramesField->setSingleStep(1.0);
    mPrevFramesField->setFixedHeight(24);
    
    mNextFramesField = new BlenderStyleValueField(this);
    mNextFramesField->setLabel(tr("Next Frames"));
    mNextFramesField->setRange(1.0, 60.0);
    mNextFramesField->setDecimals(0);
    mNextFramesField->setSingleStep(1.0);
    mNextFramesField->setFixedHeight(24);
    
    mMinOpacityField = new BlenderStyleValueField(this);
    mMinOpacityField->setLabel(tr("Min"));
    mMinOpacityField->setRange(0.0, 100.0);
    mMinOpacityField->setDecimals(0);
    mMinOpacityField->setSingleStep(1.0);
    mMinOpacityField->setFixedHeight(24);
    
    mMaxOpacityField = new BlenderStyleValueField(this);
    mMaxOpacityField->setLabel(tr("Max"));
    mMaxOpacityField->setRange(0.0, 100.0);
    mMaxOpacityField->setDecimals(0);
    mMaxOpacityField->setSingleStep(1.0);
    mMaxOpacityField->setFixedHeight(24);
    
    // Create a layout for the prev/next frame controls
    QVBoxLayout* framesLayout = new QVBoxLayout();
    framesLayout->setSpacing(3);
    framesLayout->setContentsMargins(3, 3, 3, 3);
    framesLayout->addWidget(mPrevFramesField);
    framesLayout->addWidget(mNextFramesField);
    
    // Create a layout for the opacity controls  
    QVBoxLayout* opacityLayout = new QVBoxLayout();
    opacityLayout->setSpacing(3);
    opacityLayout->setContentsMargins(3, 3, 3, 3);
    
    QLabel* opacityLabel = new QLabel(tr("Distributed Opacity"), this);
    opacityLabel->setAlignment(Qt::AlignCenter);
    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(mMinOpacityField);
    opacityLayout->addWidget(mMaxOpacityField);
    
    // Get the main layout from the UI and insert our new widgets
    // Replace the old prevFramesGroup and nextFramesGroup with our new layout
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(ui->scrollAreaWidgetContents->layout());
    if (mainLayout) {
        // Hide the old group boxes
        ui->prevFramesGroup->hide();
        ui->nextFramesGroup->hide();
        
        // Remove them from layout
        mainLayout->removeWidget(ui->prevFramesGroup);
        mainLayout->removeWidget(ui->nextFramesGroup);
        
        // Insert our new toggle layout at the top
        mainLayout->insertLayout(0, toggleLayout);
        
        // Insert frame controls
        mainLayout->insertLayout(1, framesLayout);
        
        // Find and replace the opacity group box
        int opacityGroupIndex = mainLayout->indexOf(ui->opacityGroup);
        if (opacityGroupIndex >= 0) {
            mainLayout->removeWidget(ui->opacityGroup);
            ui->opacityGroup->hide();
            mainLayout->insertLayout(opacityGroupIndex, opacityLayout);
        }
    }
    
    // Connect the new widgets
    connect(mPrevFramesToggle, &BlenderStyleToggle::toggled, this, &OnionSkinWidget::prevFramesToggled);
    connect(mNextFramesToggle, &BlenderStyleToggle::toggled, this, &OnionSkinWidget::nextFramesToggled);
    connect(mOnionColorToggle, &BlenderStyleToggle::toggled, this, &OnionSkinWidget::onionColorToggled);
    
    connect(mPrevFramesField, &BlenderStyleValueField::valueChanged, this, &OnionSkinWidget::onionPrevFramesNumChange);
    connect(mNextFramesField, &BlenderStyleValueField::valueChanged, this, &OnionSkinWidget::onionNextFramesNumChange);
    connect(mMinOpacityField, &BlenderStyleValueField::valueChanged, this, &OnionSkinWidget::onionMinOpacityChange);
    connect(mMaxOpacityField, &BlenderStyleValueField::valueChanged, this, &OnionSkinWidget::onionMaxOpacityChange);
    
    updateUI();
    makeConnections();

#ifdef __APPLE__
    ui->scrollAreaWidgetContents->layout()->setSpacing(8);

    // Mac only style. ToolButtons are naturally borderless on Win/Linux.
    QString stylesheet =
        "QToolButton { border: 0px; } "
        "QGroupBox::title {"
            "subcontrol-origin: padding;"
            "left: 6px;"
            "padding: 2px 2px 0px 0px;"
            "background: transparent;"
        "}"
        "QGroupBox {"
            "subcontrol-origin: margin;"
            "margin-top: 8px;"
            "padding-top: 16px;"
        "}"
        "QToolButton:pressed{ border: 1px solid #FFADAD; border-radius: 2px; background-color: #D5D5D5; }"
        "QToolButton:checked{ border: 1px solid #ADADAD; border-radius: 2px; background-color: #D5D5D5; }";
    setStyleSheet(this->styleSheet().append(stylesheet));
#endif
}

void OnionSkinWidget::makeConnections()
{
    connect(ui->prevFramesGroup, &QGroupBox::clicked, this, &OnionSkinWidget::prevFramesToggled);
    connect(ui->nextFramesGroup, &QGroupBox::clicked, this, &OnionSkinWidget::nextFramesToggled);
    connect(ui->onionBlueButton, &QToolButton::clicked, this, &OnionSkinWidget::onionBlueButtonClicked);
    connect(ui->onionRedButton, &QToolButton::clicked, this, &OnionSkinWidget::onionRedButtonClicked);

    connect(ui->onionSkinMode, &QCheckBox::stateChanged, this, &OnionSkinWidget::onionSkinModeChange);
    connect(ui->onionWhilePlayback, &QCheckBox::stateChanged, this, &OnionSkinWidget::playbackStateChanged);
    connect(ui->onionSkinMultiLayer, &QCheckBox::stateChanged, this, &OnionSkinWidget::onionSkinMultipleLayersEnabled);

    PreferenceManager* prefs = editor()->preference();
    connect(prefs, &PreferenceManager::optionChanged, this, &OnionSkinWidget::updateUI);

}

void OnionSkinWidget::updateUI()
{
    PreferenceManager* prefs = editor()->preference();

    // Update toggle states
    if (mPrevFramesToggle) {
        QSignalBlocker b1(mPrevFramesToggle);
        mPrevFramesToggle->setChecked(prefs->isOn(SETTING::PREV_ONION));
    }
    
    if (mNextFramesToggle) {
        QSignalBlocker b2(mNextFramesToggle);
        mNextFramesToggle->setChecked(prefs->isOn(SETTING::NEXT_ONION));
    }
    
    if (mOnionColorToggle) {
        QSignalBlocker b3(mOnionColorToggle);
        // Color toggle is on if either red or blue is on
        bool colorEnabled = prefs->isOn(SETTING::ONION_RED) || prefs->isOn(SETTING::ONION_BLUE);
        mOnionColorToggle->setChecked(colorEnabled);
    }
    
    // Update value fields
    if (mPrevFramesField) {
        QSignalBlocker b4(mPrevFramesField);
        mPrevFramesField->setValue(prefs->getInt(SETTING::ONION_PREV_FRAMES_NUM));
    }
    
    if (mNextFramesField) {
        QSignalBlocker b5(mNextFramesField);
        mNextFramesField->setValue(prefs->getInt(SETTING::ONION_NEXT_FRAMES_NUM));
    }
    
    if (mMinOpacityField) {
        QSignalBlocker b6(mMinOpacityField);
        mMinOpacityField->setValue(prefs->getInt(SETTING::ONION_MIN_OPACITY));
    }
    
    if (mMaxOpacityField) {
        QSignalBlocker b7(mMaxOpacityField);
        mMaxOpacityField->setValue(prefs->getInt(SETTING::ONION_MAX_OPACITY));
    }

    // Update old UI elements (for compatibility)
    ui->prevFramesGroup->setChecked(prefs->isOn(SETTING::PREV_ONION));
    ui->nextFramesGroup->setChecked(prefs->isOn(SETTING::NEXT_ONION));

    QSignalBlocker b8(ui->onionBlueButton);
    ui->onionBlueButton->setChecked(prefs->isOn(SETTING::ONION_BLUE));

    QSignalBlocker b9(ui->onionRedButton);
    ui->onionRedButton->setChecked(prefs->isOn(SETTING::ONION_RED));

    ui->onionMaxOpacityBox->setValue(prefs->getInt(SETTING::ONION_MAX_OPACITY));
    ui->onionMinOpacityBox->setValue(prefs->getInt(SETTING::ONION_MIN_OPACITY));
    ui->onionPrevFramesNumBox->setValue(prefs->getInt(SETTING::ONION_PREV_FRAMES_NUM));
    ui->onionNextFramesNumBox->setValue(prefs->getInt(SETTING::ONION_NEXT_FRAMES_NUM));

    QSignalBlocker b10(ui->onionSkinMode);
    ui->onionSkinMode->setChecked(prefs->getString(SETTING::ONION_TYPE) == "absolute");

    QSignalBlocker b11(ui->onionWhilePlayback);
    ui->onionWhilePlayback->setChecked(prefs->isOn(SETTING::ONION_WHILE_PLAYBACK));

    QSignalBlocker b12(ui->onionSkinMultiLayer);
    ui->onionSkinMultiLayer->setChecked(prefs->isOn(SETTING::ONION_MUTLIPLE_LAYERS));
}

void OnionSkinWidget::prevFramesToggled(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::PREV_ONION, isOn);
}

void OnionSkinWidget::nextFramesToggled(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::NEXT_ONION, isOn);
}

void OnionSkinWidget::onionColorToggled(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    // When enabled, turn on both red and blue
    // When disabled, turn off both
    prefs->set(SETTING::ONION_RED, isOn);
    prefs->set(SETTING::ONION_BLUE, isOn);
}

void OnionSkinWidget::onionRedButtonClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_RED, isOn);
}

void OnionSkinWidget::onionBlueButtonClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_BLUE, isOn);
}

void OnionSkinWidget::onionMaxOpacityChange(double value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_MAX_OPACITY, static_cast<int>(value));
}

void OnionSkinWidget::onionMinOpacityChange(double value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_MIN_OPACITY, static_cast<int>(value));
}

void OnionSkinWidget::onionPrevFramesNumChange(double value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_PREV_FRAMES_NUM, static_cast<int>(value));
}

void OnionSkinWidget::onionNextFramesNumChange(double value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_NEXT_FRAMES_NUM, static_cast<int>(value));
}

void OnionSkinWidget::onionSkinModeChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    if (value == Qt::Checked)
    {
        prefs->set(SETTING::ONION_TYPE, QString("absolute"));
    }
    else
    {
        prefs->set(SETTING::ONION_TYPE, QString("relative"));
    }
}

void OnionSkinWidget::playbackStateChanged(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_WHILE_PLAYBACK, value);
}

void OnionSkinWidget::onionSkinMultipleLayersEnabled(bool value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_MUTLIPLE_LAYERS, value);
}

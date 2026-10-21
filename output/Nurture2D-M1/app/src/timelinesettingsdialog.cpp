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

#include "timelinesettingsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include "../../CUI/cosmicinputfield.h"
#include "../../CUI/cosmicdropdown.h"

TimelineSettingsDialog::TimelineSettingsDialog(QWidget* parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowModality(Qt::NonModal);

    // Main container widget with rounded corners
    QWidget* container = new QWidget(this);
    container->setObjectName("settingsContainer");
    container->setStyleSheet(
        "#settingsContainer {"
        "    background-color: palette(window);"
        "    border: 1px solid palette(mid);"
        "    border-radius: 12px;"
        "}"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->addWidget(container);

    QVBoxLayout* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(16, 16, 16, 16);
    containerLayout->setSpacing(12);

    // Title
    QLabel* titleLabel = new QLabel(tr("Timeline Settings"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    containerLayout->addWidget(titleLabel);

    // Range section
    QGroupBox* rangeGroup = new QGroupBox(tr("Playback Range"), this);
    QVBoxLayout* rangeLayout = new QVBoxLayout(rangeGroup);
    rangeLayout->setSpacing(8);

    mRangeCheckBox = new QCheckBox(tr("Enable Range"), this);
    mRangeCheckBox->setFixedHeight(24);
    rangeLayout->addWidget(mRangeCheckBox);

    QHBoxLayout* rangeSpinLayout = new QHBoxLayout();
    rangeSpinLayout->setSpacing(8);

    QLabel* startLabel = new QLabel(tr("Start:"), this);
    mLoopStartField = new CosmicInputField(this);
    mLoopStartField->setObjectName("loopStartField");
    mLoopStartField->setVariant(CosmicInputField::Minimal);
    mLoopStartField->setFixedHeight(24);
    mLoopStartField->setRange(1, 9999);
    mLoopStartField->setValue(1);
    mLoopStartField->setEnabled(false);

    QLabel* endLabel = new QLabel(tr("End:"), this);
    mLoopEndField = new CosmicInputField(this);
    mLoopEndField->setObjectName("loopEndField");
    mLoopEndField->setVariant(CosmicInputField::Minimal);
    mLoopEndField->setFixedHeight(24);
    mLoopEndField->setRange(2, 9999);
    mLoopEndField->setValue(2);
    mLoopEndField->setEnabled(false);

    rangeSpinLayout->addWidget(startLabel);
    rangeSpinLayout->addWidget(mLoopStartField, 1);
    rangeSpinLayout->addWidget(endLabel);
    rangeSpinLayout->addWidget(mLoopEndField, 1);

    rangeLayout->addLayout(rangeSpinLayout);
    containerLayout->addWidget(rangeGroup);

    // Sound section
    QGroupBox* soundGroup = new QGroupBox(tr("Sound"), this);
    QVBoxLayout* soundLayout = new QVBoxLayout(soundGroup);
    soundLayout->setSpacing(8);

    mSoundButton = new QPushButton(tr("Sound On/Off"), this);
    mSoundButton->setCheckable(true);
    mSoundButton->setChecked(true);
    mSoundButton->setFixedHeight(32);

    mSoundScrubButton = new QPushButton(tr("Sound Scrub"), this);
    mSoundScrubButton->setCheckable(true);
    mSoundScrubButton->setFixedHeight(32);

    soundLayout->addWidget(mSoundButton);
    soundLayout->addWidget(mSoundScrubButton);
    containerLayout->addWidget(soundGroup);

    // Timeline display section
    QGroupBox* displayGroup = new QGroupBox(tr("Timeline Display"), this);
    QVBoxLayout* displayLayout = new QVBoxLayout(displayGroup);
    displayLayout->setSpacing(8);

    QHBoxLayout* timestampLayout = new QHBoxLayout();
    QLabel* timestampLabel = new QLabel(tr("Timestamp mode:"), this);
    mTimestampCombo = new QComboBox(this);
    // Order must match TimecodeTextLevel enum in pencildef.h
    mTimestampCombo->addItem(tr("No text"));                // 0 = NOTEXT
    mTimestampCombo->addItem(tr("Current Frame"));          // 1 = FRAMES
    mTimestampCombo->addItem(tr("SMPTE (MM:SS:FF)"));       // 2 = SMPTE
    mTimestampCombo->addItem(tr("FPS (S:FF)"));             // 3 = SFF
    timestampLayout->addWidget(timestampLabel);
    timestampLayout->addWidget(mTimestampCombo, 1);
    displayLayout->addLayout(timestampLayout);
    containerLayout->addWidget(displayGroup);

    // Performance section
    QGroupBox* perfGroup = new QGroupBox(tr("Performance"), this);
    QVBoxLayout* perfLayout = new QVBoxLayout(perfGroup);
    perfLayout->setSpacing(8);

    QHBoxLayout* perfLay = new QHBoxLayout();
    QLabel* perfLabel = new QLabel(tr("Playback:"), this);
    perfLabel->setStyleSheet("QLabel { color: #b2b2b2; font-size: 12px; }");
    
    mPlaybackPerfDropDown = new CosmicDropDown(this);
    mPlaybackPerfDropDown->setOptions(QStringList() << tr("Prefer Quality") << tr("Prefer Speed"));
    mPlaybackPerfDropDown->setFixedHeight(24);
    
    perfLay->addWidget(perfLabel);
    perfLay->addWidget(mPlaybackPerfDropDown, 1);
    perfLayout->addLayout(perfLay);
    containerLayout->addWidget(perfGroup);

    // Connect range checkbox to enable/disable fields
    connect(mRangeCheckBox, &QCheckBox::toggled, mLoopStartField, &CosmicInputField::setEnabled);
    connect(mRangeCheckBox, &QCheckBox::toggled, mLoopEndField, &CosmicInputField::setEnabled);

    // Set fixed size for the dialog (enlarged for better spacing and new performance section)
    setFixedSize(460, 420);

    // Add drop shadow effect
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 4);
    container->setGraphicsEffect(shadow);
}

void TimelineSettingsDialog::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    // Custom paint for rounded corners
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
}

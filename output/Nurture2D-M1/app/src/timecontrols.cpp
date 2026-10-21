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

#include "timecontrols.h"

#include <QLabel>
#include <QSettings>
#include <QMenu>
#include <QDebug>
#include <QHBoxLayout>
#include "editor.h"
#include "playbackmanager.h"
#include "layermanager.h"
#include "pencildef.h"
#include "util.h"
#include "preferencemanager.h"
#include "preferencesdef.h"
#include "timeline.h"
#include "pencildef.h"
#include "../../CUI/cosmicinputfield.h"
#include "../../CUI/blenderstylebutton.h"
#include "../../CUI/blenderstyletoggle.h"

TimeControls::TimeControls(TimeLine* parent) : QToolBar(parent)
{
    mTimeline = parent;
}

void TimeControls::setTimecodeMode(int mode)
{
    // Clamp to valid range of TimecodeTextLevel
    if (mode < NOTEXT) mode = NOTEXT;
    if (mode > SFF)    mode = SFF;

    // Write through PreferenceManager so optionChanged is emitted and all
    // listeners (including CanvasTimecodeOverlay) react immediately.
    if (mEditor && mEditor->preference()) {
        mEditor->preference()->set(SETTING::TIMECODE_TEXT, mode);
    } else {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue(SETTING_TIMECODE_TEXT, mode);
    }
    mTimecodeLabelEnum = mode;

    switch (mTimecodeLabelEnum)
    {
    case NOTEXT:
        if (mTimecodeLabel) mTimecodeLabel->setToolTip("");
        break;
    case FRAMES:
        if (mTimecodeLabel) mTimecodeLabel->setToolTip(tr("Actual frame number"));
        break;
    case SMPTE:
        if (mTimecodeLabel) mTimecodeLabel->setToolTip(tr("Timecode format MM:SS:FF"));
        break;
    case SFF:
        if (mTimecodeLabel) mTimecodeLabel->setToolTip(tr("Timecode format S:FF"));
        break;
    }

    updateTimecodeLabel(mEditor ? mEditor->currentFrame() : 0);
}

void TimeControls::initUI()
{
    QSettings settings(PENCIL2D, PENCIL2D);

    // Create CosmicInputField for FPS
    mFpsField = new CosmicInputField(this);
    mFpsField->setRange(1.0, 90.0);
    mFpsField->setValue(settings.value("Fps", 12).toDouble());
    mFpsField->setDecimals(0);
    mFpsField->setSingleStep(1.0);
    mFpsField->setSuffix(" FPS");
    mFpsField->setToolTip(tr("Frames per second"));
    mFpsField->setFixedHeight(24);

    mFps = static_cast<int>(mFpsField->value());
    mTimecodeSelect = new QToolButton(this);
    mTimecodeSelect->hide();  // Hidden - not used in toolbar

    QMenu* timeSelectMenu = new QMenu(tr("Display timecode", "Timeline menu for choose a timecode"), this);
    mTimecodeSelect->setIcon(QIcon(":/icons/themes/playful/misc/more-options.svg"));

    timeSelectMenu->addAction(mNoTimecodeAction = new QAction(tr("No text"), this));
    timeSelectMenu->addAction(mOnlyFramesAction = new QAction(tr("Frames"), this));
    timeSelectMenu->addAction(mSmpteAction = new QAction(tr("SMPTE Timecode"), this));
    timeSelectMenu->addAction(mSffAction = new QAction(tr("SFF Timecode"), this));
    mTimecodeSelect->setMenu(timeSelectMenu);
    mTimecodeSelect->setPopupMode(QToolButton::InstantPopup);
    mTimecodeSelect->setStyleSheet("::menu-indicator{ image: none; }");
    mTimecodeLabelEnum = mEditor->preference()->getInt(SETTING::TIMECODE_TEXT);
    mTimecodeLabel = new QLabel(this);
    mTimecodeLabel->setContentsMargins(2, 0, 0, 0);
    mTimecodeLabel->setMinimumWidth(72); // reduce clipping in compact layouts
    mTimecodeLabel->setText("");

    switch (mTimecodeLabelEnum)
    {
    case NOTEXT:
        mTimecodeLabel->setToolTip("");
        break;
    case FRAMES:
        mTimecodeLabel->setToolTip(tr("Actual frame number"));
        break;
    case SMPTE:
        mTimecodeLabel->setToolTip(tr("Timecode format MM:SS:FF"));
        break;
    case SFF:
        mTimecodeLabel->setToolTip(tr("Timecode format S:FF"));
        break;
    default:
        mTimecodeLabel->setToolTip("");
    }

    mLoopStartSpinBox = new QSpinBox(this);
    mLoopStartSpinBox->setObjectName("playbackRangeStart");
    mLoopStartSpinBox->setFixedHeight(24);
    mLoopStartSpinBox->setRange(1, 9998);
    mLoopStartSpinBox->setValue(settings.value("loopStart", 1).toInt()); // Default to 1
    mLoopStartSpinBox->setKeyboardTracking(false);
    mLoopStartSpinBox->setEnabled(true); // Enabled by default
    mLoopStartSpinBox->setToolTip(tr("Start of playback loop"));
    mLoopStartSpinBox->setFocusPolicy(Qt::WheelFocus);

    mLoopEndSpinBox = new QSpinBox(this);
    mLoopEndSpinBox->setObjectName("playbackRangeEnd");
    mLoopEndSpinBox->setFixedHeight(24);
    mLoopEndSpinBox->setRange(mLoopStartSpinBox->value() + 1, 9999);
    mLoopEndSpinBox->setValue(settings.value("loopEnd", 24).toInt()); // Default to 24
    mLoopEndSpinBox->setKeyboardTracking(false);
    mLoopEndSpinBox->setEnabled(true); // Enabled by default
    mLoopEndSpinBox->setToolTip(tr("End of playback loop"));
    mLoopEndSpinBox->setFocusPolicy(Qt::WheelFocus);

    // Reuse the actual range controls so marker dragging, settings and typed
    // values all follow the same playback range, without a second UI copy.
    mRangePane = new QWidget(this);
    mRangePane->setObjectName("playbackRangeControls");
    mRangePane->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    mRangePane->setStyleSheet(
        "QWidget#playbackRangeControls { background: #303030; border-radius: 5px; }"
        "QLabel { color: #b3b3b3; background: transparent; font-size: 12px; }"
        "QSpinBox { color: #eeeeee; background: #505050; border: none;"
        " border-radius: 4px; padding: 0 4px; selection-background-color: #7447ff; }"
        "QSpinBox:disabled { color: #777777; }");
    auto* rangeLayout = new QHBoxLayout(mRangePane);
    rangeLayout->setContentsMargins(8, 3, 8, 3);
    rangeLayout->setSpacing(6);
    auto* startLabel = new QLabel(tr("Start"), mRangePane);
    auto* endLabel = new QLabel(tr("End"), mRangePane);
    startLabel->setBuddy(mLoopStartSpinBox);
    endLabel->setBuddy(mLoopEndSpinBox);
    for (auto* field : {mLoopStartSpinBox, mLoopEndSpinBox}) {
        field->setButtonSymbols(QAbstractSpinBox::NoButtons);
        field->setAlignment(Qt::AlignCenter);
        field->setFixedWidth(qMax(54, field->fontMetrics().horizontalAdvance("9999") + 16));
    }
    mLoopStartSpinBox->setAccessibleName(tr("Playback start frame"));
    mLoopEndSpinBox->setAccessibleName(tr("Playback end frame"));
    mLoopStartSpinBox->setToolTip(tr("First frame to play (inclusive). Type a number and press Enter."));
    mLoopEndSpinBox->setToolTip(tr("Last frame to play (inclusive). Type a number and press Enter."));
    rangeLayout->addWidget(startLabel);
    rangeLayout->addWidget(mLoopStartSpinBox);
    rangeLayout->addWidget(endLabel);
    rangeLayout->addWidget(mLoopEndSpinBox);

    mPlaybackRangeCheckBox = new QCheckBox(tr("Range"));
    mPlaybackRangeCheckBox->setChecked(true); // Enabled by default
    mPlaybackRangeCheckBox->setFixedHeight(24);
    mPlaybackRangeCheckBox->setToolTip(tr("Playback range"));
    mPlaybackRangeCheckBox->hide();  // Hidden - moved to settings dialog

    // Create BlenderStyle playback buttons (borderless mode)
    // All buttons need same fixed size for consistent appearance
    const int buttonSize = 36;
    
    mJumpToStartButton = new BlenderStyleButton(this);
    mJumpToStartButton->setBorderless(true);
    mJumpToStartButton->setCornerMode(BlenderStyleButton::LeftCorners);
    mJumpToStartButton->setFixedSize(buttonSize, buttonSize);
    
    mPlayButton = new BlenderStyleToggle(this);
    mPlayButton->setCornerMode(BlenderStyleToggle::NoCorners);
    mPlayButton->setBorderSides(BlenderStyleToggle::BorderNone);
    mPlayButton->setFixedSize(buttonSize, buttonSize);
    
    mJumpToEndButton = new BlenderStyleButton(this);
    mJumpToEndButton->setBorderless(true);
    mJumpToEndButton->setCornerMode(BlenderStyleButton::NoCorners);
    mJumpToEndButton->setFixedSize(buttonSize, buttonSize);
    
    // Create BlenderStyle toggle for loop (merged with buttons)
    mLoopToggle = new BlenderStyleToggle(this);
    mLoopToggle->setCornerMode(BlenderStyleToggle::RightCorners);
    mLoopToggle->setBorderSides(BlenderStyleToggle::BorderNone);  // No borders for seamless merge
    mLoopToggle->setFixedSize(buttonSize, buttonSize);
    
    mSoundButton = new QPushButton(this);
    mSoundButton->setIconSize(QSize(22,22));
    mSoundButton->hide();  // Hidden - moved to settings dialog
    mSoundScrubButton = new QPushButton(this);
    mSoundScrubButton->setIconSize(QSize(22,22));
    mSoundScrubButton->hide();  // Hidden - moved to settings dialog

    mLoopIcon = QIcon(":icons/themes/playful/controls/control-loop.svg");
    mSoundIcon = QIcon(":icons/themes/playful/controls/control-sound-enable.svg");
    mSoundScrubIcon = QIcon(":icons/themes/playful/controls/control-sound-scrub.svg");
    mJumpToEndIcon = QIcon(":icons/themes/playful/controls/control-play-end.svg");
    mJumpToStartIcon = QIcon(":icons/themes/playful/controls/control-play-start.svg");
    mStartIcon = QIcon(":icons/themes/playful/controls/control-play.svg");
    mStopIcon = QIcon(":icons/themes/playful/controls/control-stop.svg");
    
    mPlayButton->setIcon(mStartIcon.pixmap(QSize(22, 22)));
    mLoopToggle->setIcon(mLoopIcon.pixmap(QSize(22, 22)));
    mSoundButton->setIcon(mSoundIcon);
    mSoundScrubButton->setIcon(mSoundScrubIcon);
    mJumpToEndButton->setIcon(mJumpToEndIcon, QSize(22, 22));
    mJumpToStartButton->setIcon(mJumpToStartIcon, QSize(22, 22));

    mPlayButton->setToolTip(tr("Play"));
    mLoopToggle->setToolTip(tr("Loop"));
    mSoundButton->setToolTip(tr("Sound on/off"));
    mSoundScrubButton->setToolTip(tr("Sound scrub on/off"));
    mJumpToEndButton->setToolTip(tr("Jump to the End", "Tooltip of the jump to end button"));
    mJumpToStartButton->setToolTip(tr("Jump to the Start", "Tooltip of the jump to start button"));

    mSoundButton->setCheckable(true);
    mSoundButton->setChecked(true);
    mSoundScrubButton->setCheckable(true);
    mSoundScrubButton->setChecked(mEditor->preference()->isOn(SETTING::SOUND_SCRUB_ACTIVE));

    // Add buttons directly to toolbar with no spacing for seamless merge
    setStyleSheet("QToolBar { spacing: 0px; }");
    addWidget(mJumpToStartButton);
    addWidget(mPlayButton);
    addWidget(mJumpToEndButton);
    addWidget(mLoopToggle);
    // Trailing pane groups all post-loop controls; not added here, placed by parent layout
    mTrailingPane = new QWidget(this);
    auto* trailingLayout = new QHBoxLayout(mTrailingPane);
    trailingLayout->setContentsMargins(0,0,0,0);
    trailingLayout->setSpacing(6);
    trailingLayout->addWidget(mFpsField);
    // Range and sound controls moved to timeline settings dialog
    // Timecode label relocated to canvas top-left overlay (CanvasTimecodeOverlay)
    // trailingLayout->addWidget(mPlaybackRangeCheckBox);
    // trailingLayout->addWidget(mLoopStartSpinBox);
    // trailingLayout->addWidget(mLoopEndSpinBox);
    // trailingLayout->addWidget(mSoundButton);
    // trailingLayout->addWidget(mSoundScrubButton);
    // trailingLayout->addWidget(mTimecodeSelect);
    // trailingLayout->addWidget(mTimecodeLabel); // moved to CanvasTimecodeOverlay
    if (mTimecodeLabel) mTimecodeLabel->hide();

    makeConnections();

    updateUI();

    // Ensure timestamp is visible immediately at startup
    if (mEditor)
        updateTimecodeLabel(mEditor->currentFrame());
}

void TimeControls::updateUI()
{
    PlaybackManager* playback = mEditor->playback();

    mPlaybackRangeCheckBox->setChecked(playback->isRangedPlaybackOn()); // don't block this signal since it enables start/end range spinboxes.

    QSignalBlocker b1(mLoopStartSpinBox);
    mLoopStartSpinBox->setValue(playback->markInFrame());

    QSignalBlocker b2(mLoopEndSpinBox);
    // A previously opened document may have had a later start frame.
    mLoopEndSpinBox->setMinimum(mLoopStartSpinBox->value() + 1);
    mLoopEndSpinBox->setValue(playback->markOutFrame());

    QSignalBlocker b3(mFpsField);
    mFpsField->setValue(playback->fps());

    QSignalBlocker b4(mLoopToggle);
    mLoopToggle->setChecked(playback->isLooping());
}

void TimeControls::setEditor(Editor* editor)
{
    Q_ASSERT(editor != nullptr);
    mEditor = editor;
}

void TimeControls::setFps(int value)
{
    QSignalBlocker blocker(mFpsField);
    mFpsField->setValue(value);
    mFps = value;
    updateTimecodeLabel(mEditor->currentFrame());
}

void TimeControls::setLoop(bool checked)
{
    mLoopToggle->setChecked(checked);
}

void TimeControls::setRangeState(bool checked)
{
    mPlaybackRangeCheckBox->setChecked(checked);
    mTimeline->updateLength();
}

void TimeControls::makeConnections()
{
    connect(mPlayButton, &BlenderStyleToggle::clicked, this, &TimeControls::playButtonClicked);
    connect(mJumpToEndButton, &BlenderStyleButton::clicked, this, &TimeControls::jumpToEndButtonClicked);
    connect(mJumpToStartButton, &BlenderStyleButton::clicked, this, &TimeControls::jumpToStartButtonClicked);
    connect(mLoopToggle, &BlenderStyleToggle::toggled, this, &TimeControls::loopButtonClicked);
    connect(mPlaybackRangeCheckBox, &QCheckBox::clicked, this, &TimeControls::playbackRangeClicked);

    auto spinBoxValueChanged = static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged);
    connect(mLoopStartSpinBox, spinBoxValueChanged, this, &TimeControls::loopStartValueChanged);
    clearFocusOnFinished(mLoopStartSpinBox);
    connect(mLoopEndSpinBox, spinBoxValueChanged, this, &TimeControls::loopEndValueChanged);
    clearFocusOnFinished(mLoopEndSpinBox);

    connect(mPlaybackRangeCheckBox, &QCheckBox::toggled, mLoopStartSpinBox, &QSpinBox::setEnabled);
    connect(mPlaybackRangeCheckBox, &QCheckBox::toggled, mLoopEndSpinBox, &QSpinBox::setEnabled);

    connect(mSoundButton, &QPushButton::clicked, this, &TimeControls::soundToggled);

    connect(mSoundScrubButton, &QPushButton::clicked, this, &TimeControls::soundScrubToggled);
    connect(mSoundScrubButton, &QPushButton::clicked, this, &TimeControls::updateSoundScrubIcon);

    // Connect CosmicInputField for FPS (converts double to int)
    connect(mFpsField, &CosmicInputField::valueChanged, this, [this](double value) {
        int fps = static_cast<int>(value);
        emit fpsChanged(fps);
        mFps = fps;
    });
    
    connect(mEditor, &Editor::fpsChanged, this, &TimeControls::setFps);
    connect(mNoTimecodeAction, &QAction::triggered, this, &TimeControls::noTimecodeText);
    connect(mOnlyFramesAction, &QAction::triggered, this, &TimeControls::onlyFramesText);
    connect(mSmpteAction, &QAction::triggered, this, &TimeControls::smpteText);
    connect(mSffAction, &QAction::triggered, this, &TimeControls::sffText);
}

void TimeControls::playButtonClicked()
{
    emit playButtonTriggered();
}

void TimeControls::updatePlayState()
{
    bool isPlaying = mEditor->playback()->isPlaying();
    
    // Block signals to prevent triggering playback when updating state
    QSignalBlocker blocker(mPlayButton);
    
    if (isPlaying)
    {
        mPlayButton->setChecked(true);
        mPlayButton->setIcon(mStopIcon.pixmap(QSize(22, 22)));
        mPlayButton->setToolTip(tr("Stop"));
    }
    else
    {
        mPlayButton->setChecked(false);
        mPlayButton->setIcon(mStartIcon.pixmap(QSize(22, 22)));
        mPlayButton->setToolTip(tr("Play"));
    }
}

void TimeControls::jumpToStartButtonClicked()
{
    if (mPlaybackRangeCheckBox->isChecked())
    {
        mEditor->scrubTo(mLoopStartSpinBox->value());
        mEditor->playback()->setCheckForSoundsHalfway(true);
    }
    else
    {
        mEditor->scrubTo(mEditor->layers()->firstKeyFrameIndex());
    }
    mEditor->playback()->stopSounds();
}

void TimeControls::jumpToEndButtonClicked()
{
    if (mPlaybackRangeCheckBox->isChecked())
    {
        mEditor->scrubTo(mLoopEndSpinBox->value());
    }
    else
    {
        mEditor->scrubTo(mEditor->layers()->lastKeyFrameIndex());
    }
}

void TimeControls::loopButtonClicked(bool bChecked)
{
    mEditor->playback()->setLooping(bChecked);
}

void TimeControls::playbackRangeClicked(bool bChecked)
{
    mEditor->playback()->enableRangedPlayback(bChecked);
}

void TimeControls::loopStartValueChanged(int i)
{
    if (i >= mLoopEndSpinBox->value())
    {
        mLoopEndSpinBox->setValue(i + 1);
    }
    mLoopEndSpinBox->setMinimum(i + 1);

    mEditor->playback()->setRangedStartFrame(i);
    qInfo() << "[PlaybackRange] Start changed:" << i
            << "end:" << mLoopEndSpinBox->value();
    mTimeline->updateLength();
}

void TimeControls::loopEndValueChanged(int i)
{
    mEditor->playback()->setRangedEndFrame(i);
    qInfo() << "[PlaybackRange] End changed:" << i
            << "start:" << mLoopStartSpinBox->value();
    mTimeline->updateLength();
}

void TimeControls::updateSoundScrubIcon(bool soundScrubEnabled)
{
    mEditor->playback()->setSoundScrubActive(soundScrubEnabled);
    mEditor->preference()->set(SETTING::SOUND_SCRUB_ACTIVE, soundScrubEnabled);
}

void TimeControls::noTimecodeText()
{
    if (mEditor && mEditor->preference())
        mEditor->preference()->set(SETTING::TIMECODE_TEXT, (int)NOTEXT);
    else { QSettings s(PENCIL2D, PENCIL2D); s.setValue(SETTING_TIMECODE_TEXT, NOTEXT); }
    mTimecodeLabelEnum = NOTEXT;
    mTimecodeLabel->setToolTip(tr(""));
    updateTimecodeLabel(mEditor->currentFrame());
}

void TimeControls::onlyFramesText()
{
    if (mEditor && mEditor->preference())
        mEditor->preference()->set(SETTING::TIMECODE_TEXT, (int)FRAMES);
    else { QSettings s(PENCIL2D, PENCIL2D); s.setValue(SETTING_TIMECODE_TEXT, FRAMES); }
    mTimecodeLabelEnum = FRAMES;
    mTimecodeLabel->setToolTip(tr("Actual frame number"));
    updateTimecodeLabel(mEditor->currentFrame());
}

void TimeControls::sffText()
{
    if (mEditor && mEditor->preference())
        mEditor->preference()->set(SETTING::TIMECODE_TEXT, (int)SFF);
    else { QSettings s(PENCIL2D, PENCIL2D); s.setValue(SETTING_TIMECODE_TEXT, SFF); }
    mTimecodeLabelEnum = SFF;
    mTimecodeLabel->setToolTip(tr("Timecode format S:FF"));
    updateTimecodeLabel(mEditor->currentFrame());
}

void TimeControls::smpteText()
{
    if (mEditor && mEditor->preference())
        mEditor->preference()->set(SETTING::TIMECODE_TEXT, (int)SMPTE);
    else { QSettings s(PENCIL2D, PENCIL2D); s.setValue(SETTING_TIMECODE_TEXT, SMPTE); }
    mTimecodeLabelEnum = SMPTE;
    mTimecodeLabel->setToolTip(tr("Timecode format MM:SS:FF"));
    updateTimecodeLabel(mEditor->currentFrame());
}

void TimeControls::onFpsEditingFinished()
{
    // No longer needed with CosmicInputField
    // Value changes are handled automatically through valueChanged signal
    if (mFpsField) {
        mFpsField->clearFocus();
    }
}

void TimeControls::updateTimecodeLabel(int frame)
{
    // Label is relocated to CanvasTimecodeOverlay; keep internal state updated
    // but do not show the label widget in the timeline toolbar.
    if (!mTimecodeLabel) return;

    switch (mTimecodeLabelEnum)
    {
    case TimecodeTextLevel::SMPTE:
        mTimecodeLabel->setText(QString("%1:%2:%3")
                                .arg(frame / (60 * mFps) % 60, 2, 10, QLatin1Char('0'))
                                .arg(frame / mFps % 60, 2, 10, QLatin1Char('0'))
                                .arg(frame % mFps, 2, 10, QLatin1Char('0')));
        break;
    case TimecodeTextLevel::SFF:
        mTimecodeLabel->setText(QString("%1:%2")
                                .arg(frame / mFps)
                                .arg(frame % mFps, 2, 10, QLatin1Char('0')));
        break;
    case TimecodeTextLevel::FRAMES:
        mTimecodeLabel->setText(QString::number(frame).rightJustified(4, '0'));
        break;
    case TimecodeTextLevel::NOTEXT:
    default:
        break;
    }
}

void TimeControls::updateLength(int frameLength)
{
    // Numeric entry can extend the timeline beyond its currently visible
    // length; TimeLine::checkTimelineExpansion grows it up to 9999 frames.
    Q_UNUSED(frameLength);
}

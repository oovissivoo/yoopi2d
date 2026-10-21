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
#ifndef TIMECONTROL_H
#define TIMECONTROL_H

#include <QWidget>
#include <QToolBar>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>

class Editor;
class PreferenceManager;
class TimeLine;
class CosmicInputField;
class BlenderStyleButton;
class BlenderStyleToggle;

class TimeControls : public QToolBar
{
    Q_OBJECT

public:
    TimeControls(TimeLine* parent = nullptr);
    void initUI();
    void updateUI();
    void setEditor(Editor* editor);
    void setFps(int value);
    void setLoop(bool);
    void setRangeState(bool);
    QWidget* trailingWidget() const { return mTrailingPane; }
    QWidget* rangeWidget() const { return mRangePane; }

    void updateLength(int frameLength);
    void updatePlayState();
    void setTimecodeMode(int mode);

    int getRangeLower() { return mPlaybackRangeCheckBox->isChecked() ? mLoopStartSpinBox->value() : -1; }
    int getRangeUpper() { return mPlaybackRangeCheckBox->isChecked() ? mLoopEndSpinBox->value() : -1; }
    void setRangeLower(int frame) { mLoopStartSpinBox->setValue(frame); }
    void setRangeUpper(int frame) { mLoopEndSpinBox->setValue(frame); }

signals:
    void soundToggled(bool);
    void soundScrubToggled(bool);
    void fpsChanged(int);
    void playButtonTriggered();

public slots:
    /// Work-around in case the FPS spin-box "valueChanged" signal doesn't work.
    void onFpsEditingFinished();
    void updateTimecodeLabel(int frame);

private:
    void makeConnections();

    void playButtonClicked();
    void jumpToStartButtonClicked();
    void jumpToEndButtonClicked();
    void loopButtonClicked(bool bChecked);
    void playbackRangeClicked(bool bChecked);
    void loopStartValueChanged(int);
    void loopEndValueChanged(int);
    void updateSoundScrubIcon(bool soundScrubEnabled);

    void noTimecodeText();
    void onlyFramesText();
    void sffText();
    void smpteText();

private:
    BlenderStyleToggle* mPlayButton = nullptr;
    BlenderStyleButton* mJumpToEndButton = nullptr;
    BlenderStyleButton* mJumpToStartButton = nullptr;
    BlenderStyleToggle* mLoopToggle = nullptr;
    QPushButton* mSoundButton = nullptr;
    QPushButton* mSoundScrubButton = nullptr;
    CosmicInputField* mFpsField = nullptr;
    QSpinBox*    mFpsBox = nullptr;  // Legacy, kept for compatibility
    QCheckBox*   mPlaybackRangeCheckBox = nullptr;
    QSpinBox*    mLoopStartSpinBox = nullptr;
    QSpinBox*    mLoopEndSpinBox = nullptr;
    QToolButton* mTimecodeSelect = nullptr;
    QLabel*      mTimecodeLabel = nullptr;
    QAction*     mNoTimecodeAction = nullptr;
    QAction*     mOnlyFramesAction = nullptr;
    QAction*     mSmpteAction = nullptr;
    QAction*     mSffAction = nullptr;
    // Timecode label is now placed inside a trailing container, not as a toolbar action
    QAction*     mTimecodeLabelAction = nullptr; // kept for backward-compatibility, unused
    QWidget*     mTrailingPane = nullptr;
    QWidget*     mRangePane = nullptr;

    QIcon mStartIcon;
    QIcon mStopIcon;
    QIcon mLoopIcon;
    QIcon mSoundIcon;
    QIcon mSoundScrubIcon;
    QIcon mJumpToEndIcon;
    QIcon mJumpToStartIcon;

    TimeLine* mTimeline = nullptr;
    Editor* mEditor = nullptr;
    int mFps = 12;
    int mTimecodeLabelEnum;
};

#endif

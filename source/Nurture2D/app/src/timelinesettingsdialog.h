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

#ifndef TIMELINESETTINGSDIALOG_H
#define TIMELINESETTINGSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>

class CosmicInputField;
class CosmicDropDown;

class TimelineSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimelineSettingsDialog(QWidget* parent = nullptr);

    // Accessors for controls
    QCheckBox* rangeCheckBox() const { return mRangeCheckBox; }
    CosmicInputField* loopStartField() const { return mLoopStartField; }
    CosmicInputField* loopEndField() const { return mLoopEndField; }
    QPushButton* soundButton() const { return mSoundButton; }
    QPushButton* soundScrubButton() const { return mSoundScrubButton; }
    QComboBox* timestampComboBox() const { return mTimestampCombo; }
    CosmicDropDown* playbackPerfDropDown() const { return mPlaybackPerfDropDown; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QCheckBox* mRangeCheckBox = nullptr;
    CosmicInputField* mLoopStartField = nullptr;
    CosmicInputField* mLoopEndField = nullptr;
    QPushButton* mSoundButton = nullptr;
    QPushButton* mSoundScrubButton = nullptr;
    QComboBox* mTimestampCombo = nullptr;
    CosmicDropDown* mPlaybackPerfDropDown = nullptr;
};

#endif // TIMELINESETTINGSDIALOG_H

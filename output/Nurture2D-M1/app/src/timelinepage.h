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

#ifndef TIMELINEPAGE_H
#define TIMELINEPAGE_H

class PreferenceManager;
class BlenderStyleValueField;
class CosmicDropDown;
class CosmicInputField;
class CosmicCheckboxToggle;
class QLabel;

namespace Ui {
class TimelinePage;
}

class TimelinePage : public QWidget
{
Q_OBJECT
public:
    TimelinePage();
    ~TimelinePage() override;

    void setManager(PreferenceManager* p) { mManager = p; }
    void updateAccentColors();

public slots:
    void updateValues();

    void timelineLengthChanged(int);
    void fontSizeChanged(int);
    void scrubChanged(int);
    void drawEmptyKeyRadioButtonToggled(bool);
    void flipRollMsecSliderChanged(int value);
    void flipRollMsecSpinboxChanged(int value);
    void flipRollNumDrawingsSliderChanged(int value);
    void flipRollNumDrawingsSpinboxChanged(int value);
    void flipInbetweenMsecSliderChanged(int value);
    void flipInbetweenMsecSpinboxChanged(int value);
    void soundScrubActiveChanged(int i);
    void soundScrubMsecSliderChanged(int value);
    void soundScrubMsecSpinboxChanged(int value);
    void layerVisibilityChanged(int);
    void layerVisibilityThresholdChanged(int);

signals:
    void soundScrubChanged(bool b);
    void soundScrubMsecChanged(int mSec);

protected:
    void changeEvent(QEvent* e) override;

private:
    void retranslatePageStrings();

    Ui::TimelinePage* ui = nullptr;
    PreferenceManager* mManager = nullptr;
    CosmicDropDown* mLayerVisibilityDropDown = nullptr;
    CosmicCheckboxToggle* mScrubToggle = nullptr;
    CosmicInputField* mTimelineLengthField = nullptr;
    CosmicInputField* mFlipRollDrawingsField = nullptr;
    CosmicInputField* mFlipRollMsecsField = nullptr;
    CosmicInputField* mFlipInbetweenMsecsField = nullptr;
    CosmicInputField* mSoundScrubMsecsField = nullptr;
    CosmicInputField* mVisibilityThresholdField = nullptr;

    // Section header labels — stored for retranslation
    QLabel* mHeaderTimeline       = nullptr;
    QLabel* mHeaderDrawing        = nullptr;
    QLabel* mHeaderFlipRoll       = nullptr;
    QLabel* mHeaderSoundScrub     = nullptr;
    QLabel* mHeaderLayerVis       = nullptr;
};

#endif // TIMELINEPAGE_H

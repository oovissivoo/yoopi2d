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
#ifndef TIMELINE_H
#define TIMELINE_H

#include "basedockwidget.h"
#include <QIcon>

class QScrollBar;
class Editor;
class TimeLineCells;
class TimeControls;
class SceneSequenceManager;
class QScrollArea;
class QPushButton;
class QLabel;
class QWidget;
class FlowLayout;
class QGridLayout;
class QFrame;

class QWheelEvent;
class QCheckBox;
class TimelineSettingsPopup;
class BlenderStyleToggle;
class BlenderStyleButton;
class CosmicButton;
class CosmicCheckboxToggle;
class CosmicDropDown;
class CosmicHSlider;
class Object;

template <class Key, class T>
class QHash;

class TimeLine : public BaseDockWidget
{
    Q_OBJECT

public:
    TimeLine( QWidget* parent );
    ~TimeLine() override;

    SceneSequenceManager* sceneSequenceManager() const { return mSceneSequenceManager; }

    bool isScenesAndShotsTabActive() const;

    void initUI() override;
    void updateUI() override;
    void updateUICached();

    void updateFrame( int frameNumber );
    void updateLayerNumber( int number );
    void updateLayerView();
    void updateLength();
    void updateContent();
    void setLoop( bool loop );
    void setRangeState( bool range );
    void setPlaying( bool isPlaying );

    void extendLength(int frame);

    int getRangeLower();
    int getRangeUpper();
    void setRangeLower(int frame);
    void setRangeUpper(int frame);

    void onObjectLoaded();
    void onCurrentLayerChanged();
    void onScrollbarValueChanged();

signals:
    void selectionChanged();
    void modification();

    void deleteCurrentLayerClick();

    void insertKeyClick();
    void removeKeyClick();
    void makeUniqueDrawingClick();

    void newBitmapLayer();
    void newVectorLayer();
    // Removed sound layer creation signal
    void newCameraLayer();

    void soundClick( bool );
    void fpsChanged( int );
    void playButtonTriggered();

public:
    bool scrubbing = false;

    // Shortcut-triggered retime actions (same logic as the bottom-bar buttons).
    // spacing = frames between keyframes (1/2/3); ripple = shift trailing frames.
    void retimeSelectedFrames(int spacing, bool ripple);

    // Shortcut-triggered layer navigation (select the layer above/below current).
    void selectLayerAbove();
    void selectLayerBelow();

protected:
    void resizeEvent( QResizeEvent* event ) override;
    void wheelEvent( QWheelEvent* ) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void updateVerticalScrollbarPosition();
    void onDimInactiveLayersToggled(bool checked);
    void onShiftAndTraceToggled(bool checked);
    void onTimelineSettingsClicked();
    void applyScrollbarSettings(int hMode, int vMode);

    void onCellBankToggled(bool checked);

    void onPlayToggleClicked();
    void onPlaybackManagerPlayStateChanged(bool isPlaying);

    void rebuildScenesAndShotsUI();
    QString projectDisplayName() const;

    QScrollBar* mHScrollbar = nullptr;
    QScrollBar* mHScrollbarTop = nullptr;
    QWidget* mLeftTopSpacer = nullptr;
    QScrollBar* mVScrollbar = nullptr;
    QScrollBar* mVScrollbarRight = nullptr;
    TimeLineCells* mTracks = nullptr;
    TimeLineCells* mLayerList = nullptr;
    TimeControls* mTimeControls = nullptr;

    QTimer* mScrollingStoppedTimer = nullptr;

    CosmicButton* mLayerDeleteButton = nullptr;
    CosmicHSlider* mLayersHeaderOpacitySlider = nullptr;
    BlenderStyleButton* mTimelineSettingsButton = nullptr;
    CosmicCheckboxToggle* mDimInactiveLayersCheckBox = nullptr;
    CosmicButton* mShiftAndTraceToggleBtn = nullptr;   // on/off toggle (left, rounded left corners)
    CosmicButton* mShiftAndTraceSettingsBtn = nullptr; // popup settings (right, rounded right corners)
    CosmicCheckboxToggle* mCellBankToggle = nullptr;  // kept for ABI; no longer used in timeline
    TimelineSettingsPopup* mSettingsDialog = nullptr;
    CosmicButton* mFirstFrameButton = nullptr;
    CosmicButton* mPrevKeyframeButton = nullptr;
    CosmicCheckboxToggle* mPlayToggle = nullptr;
    CosmicButton* mNextKeyframeButton = nullptr;
    CosmicButton* mLastFrameButton = nullptr;
    CosmicCheckboxToggle* mLoopToggle = nullptr;

    QIcon mPlayIcon;
    QIcon mStopIcon;
    int mNumLayers = 0;
    int mLastUpdatedFrame = 0;
    int mContentLength = 0;

    QWidget* mCellBankContainer = nullptr;  // kept for ABI; no longer used in timeline
    class CellStackWidget* mCellBankWidget = nullptr;  // kept for ABI; no longer used in timeline

    void checkTimelineExpansion(int currentFrame);

    SceneSequenceManager* mSceneSequenceManager = nullptr;
    QWidget* mScenesAndShotsTab = nullptr;
    QWidget* mScenesLeftPanel = nullptr;
    QWidget* mScenesRightPanel = nullptr;
    QPushButton* mBreadcrumbSceneButton = nullptr;
    QLabel* mBreadcrumbSequenceLabel = nullptr;
    QScrollArea* mScenesScrollArea = nullptr;
    QWidget* mScenesGridContainer = nullptr;
    QWidget* mScenesFlowWrap = nullptr;
    QGridLayout* mScenesGridLayout = nullptr;
    QScrollBar* mScenesVScrollbar = nullptr;
    bool mScenesRebuildQueued = false;
    bool mScenesAndShotsRebuildDeferred = false;

    QTimer* mShotThumbInvalidateTimer = nullptr;
    QHash<const Object*, quint32>* mShotThumbRevisions = nullptr;

    QTimer* mShotHoverTimer = nullptr;
    QWidget* mHoveredShotPill = nullptr;
    int mHoverFrame = 1;
    int mHoverMaxFrame = 1;

    CosmicButton* mScenesAddButton = nullptr;
    CosmicButton* mScenesRemoveButton = nullptr;

    QLabel* mPlaybackTypeLabel = nullptr;
    CosmicDropDown* mPlaybackTypeDropDown = nullptr;
    CosmicDropDown* mTagModeDropDown = nullptr;

    QLabel* mPlaybackPerfLabel = nullptr;
    CosmicDropDown* mPlaybackPerfDropDown = nullptr;

    bool mShotPlaylistActive = false;
    bool mShotPlaylistAdvancing = false;
    int mShotPlaylistMode = 0; // 0=Shot, 1=From Current, 2=All
    int mShotPlaylistSceneIndex = 0;
    int mShotPlaylistSequenceIndex = 0;
    int mShotPlaylistLoopStartSceneIndex = 0;
    int mShotPlaylistLoopStartSequenceIndex = 0;
    bool mShotPlaylistLoopingEnabled = false;
    bool mShotPlaylistForcingPlaybackLoopOff = false;
    bool mShotPlaylistStopRequested = false;
    bool mShotPlaylistQueuedPlay = false;
    int mShotPlaylistOriginalSceneIndex = 0;
    int mShotPlaylistOriginalSequenceIndex = 0;
    bool mShotPlaylistOriginalAtSceneLevel = true;
    int mShotPlaylistOriginalBrowsingSceneIndex = 0;
    bool mShotPlaylistOriginalLooping = false;

    bool mReorderArmed = false;
    bool mReorderDragging = false;
    QString mReorderKind;
    int mReorderSceneIndex = 0;
    int mReorderPressedIndex = 0;
    int mReorderInsertPos = 0;
    QPoint mReorderPressGlobalPos;
    QFrame* mReorderDropIndicator = nullptr;

    int mSelectedSceneIndex = 0;
    int mSelectedSequenceIndex = 0;

    int mActiveTabIndex = 0;
};

#endif

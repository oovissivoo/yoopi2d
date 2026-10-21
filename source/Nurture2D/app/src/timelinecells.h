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

#ifndef TIMELINECELLS_H
#define TIMELINECELLS_H

#include <QString>
#include <QWidget>
#include <QVector>
#include <QHash>
#include <QColor>
#include <QElapsedTimer>
#include "layercamera.h"
class QLineEdit;
class QEvent;
class QTimer;

class Layer;
enum class LayerVisibility;
class TimeLine;
class QPaintEvent;
class QMouseEvent;
class QResizeEvent;
class QAbstractButton;
class Editor;
class PreferenceManager;
class QMenu;
class QAction;
enum class SETTING;
class CosmicCheckboxToggle;
class CosmicHSlider;

enum class TIMELINE_CELL_TYPE
{
    Layers,
    Tracks
};

class TimeLineCells : public QWidget
{
    Q_OBJECT

public:
    TimeLineCells( TimeLine* parent, Editor* editor, TIMELINE_CELL_TYPE );
    ~TimeLineCells() override;

    static int layerTagDefCount();
    static QString layerTagDefName(int index);
    static QColor layerTagDefColor(int index);

    static bool isTagFilterEnabled(int tagIndex);
    static void setTagFilterEnabled(int tagIndex, bool enabled);
    static bool isDefaultLayerFilterEnabled();
    static void setDefaultLayerFilterEnabled(bool enabled);
    static void resetTagFilters();

    static bool isTagVisibilityEnabled(int tagIndex);
    static void setTagVisibilityEnabled(int tagIndex, bool enabled, Editor* editor);
    static bool isDefaultLayerVisibilityEnabled();
    static void setDefaultLayerVisibilityEnabled(bool enabled, Editor* editor);
    static void resetTagVisibility(Editor* editor);

    static QHash<int, int> layerTagByLayerId();
    static void setLayerTagByLayerId(const QHash<int, int>& map);
    static void clearLayerTags();

    static int getOffsetX() { return mOffsetX; }
    static int getOffsetY() { return mOffsetY; }
    int getLayerHeight() const { return mLayerHeight; }

    int getFrameLength() const { return mFrameLength; }
    int getFrameSize() const { return mFrameSize; }

    void setFrameLength(int n) { mFrameLength = n; }
    void setFrameSize(int size);
    void clearCache() { delete mCache; mCache = nullptr; }

    bool didDetachLayer() const;

    void showCameraMenu(QPoint pos);
    void showLayerMenu(QPoint pos);

    // New helpers for camera-hiding behavior in the timeline UI
    int getVisibleLayerCount() const;
    int toVisibleIndex(int actualIndex) const;
    bool isLayerVisibleInTimeline(int actualIndex) const;
    int findNearestVisibleLayerIndex(int preferredActualIndex) const;
    QVector<int> getVisibleLayerIndices() const;

signals:
    void mouseMovedY(int);
    void lengthChanged(int);
    void offsetChanged(int);
    void selectionChanged();
    void insertNewKeyFrame();

public slots:
    void updateContent();
    void updateFrame(int frameNumber);
    void hScrollChange(int);
    void vScrollChange(int);
    void onScrollingVerticallyStopped();
    void setMouseMoveY(int x);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    static void ensureLayerTagFilterStateSize();
    static void ensureLayerTagVisibilityStateSize();
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void loadSetting(SETTING setting);

private:
    struct LayerRowControls
    {
        CosmicCheckboxToggle* visibilityToggle = nullptr;
        QAbstractButton* stubToggle1 = nullptr;
        int layerIndex = -1;
    };

    void ensureLayerRowControls();
    void updateLayerRowControls();
    void showLayerTagPopup(int layerIndex, const QPoint& globalAnchorPos);

    int getLayerNumber(int y) const;
    int getInbetweenLayerNumber(int y) const;
    int getLayerY(int layerNumber) const;
    int getFrameX(int frameNumber) const;
    int getFrameNumber(int x) const;

    // Visible (non-camera) layer utilities — implementation in timelinecells.cpp

    void onDidLeaveWidget();

    bool trackScrubber();
    void drawContent();
    void paintTicks(QPainter& painter, const QPalette& palette) const;
    void paintOnionSkin(QPainter& painter) const;
    void paintLayerGutter(QPainter& painter) const;
    void paintTrack(QPainter& painter, const Layer* layer, int x, int y, int width, int height, bool selected, int frameSize) const;
    void paintFrames(QPainter& painter, QColor trackCol, const Layer* layer, int y, int height, bool selected, int frameSize) const;
    void paintCurrentFrameBorder(QPainter& painter, int recLeft, int recTop, int recWidth, int recHeight) const;
    void paintFrameCursorOnCurrentLayer(QPainter& painter, int recTop, int recWidth, int recHeight) const;
    void paintSelectedFrames(QPainter& painter, const Layer* layer, const int layerIndex) const;
    void paintLabel(QPainter& painter, const Layer* layer, int x, int y, int width, int height, bool selected, LayerVisibility layerVisibility) const;
    void paintSelection(QPainter& painter, int x, int y, int width, int height) const;
    void paintHighlightedFrame(QPainter& painter, int framePos, int recTop, int recWidth, int recHeight) const;
    void paintRangeMarkers(QPainter& painter) const;
    void paintRangeDimming(QPainter& painter) const;
    bool isOverRangeStartMarker(int x) const;
    bool isOverRangeEndMarker(int x) const;

    bool hasSelectedFramesForAnimation() const;
    void paintAnimatedSelectedFramesOverlay(QPainter& painter) const;

    int getLayerInsertSlot(int y) const;

    void editLayerProperties(Layer* layer) const;
    void editLayerProperties(LayerCamera *layer) const;
    void editLayerName(Layer* layer) const;

    // Inline rename helpers
    void startInlineRename(int layerIndex);
    void finishInlineRename(bool accept);
    QRect inlineRenameRectForLayer(int layerIndex) const;
    void updateInlineRenameGeometry();

    // Inline cell ID edit helpers (bitmap tracks)
    void startInlineCellEdit(int layerIndex, int frameNumber);
    void finishInlineCellEdit(bool accept);

    TimeLine* mTimeLine;
    Editor* mEditor; // the editor for which this timeLine operates
    PreferenceManager* mPrefs;

    TIMELINE_CELL_TYPE mType;

    QPixmap* mCache = nullptr;
    bool mRedrawContent = false;
    bool mDrawFrameNumber = true;
    bool mbShortScrub = false;
    int mFrameLength = 1;
    int mFrameSize = 0;
    int mFontSize = 10;
    bool mScrubbing = false;
    bool mHighlightFrameEnabled = false;
    int mHighlightedFrame = -1;
    int mLayerHeight = 30;
    int mStartY = 0;
    int mEndY   = 0;

    int mCurrentLayerNumber = 0;
    int mLastScrubFrame = 0;

    int mFromLayer = 0;
    int mToLayer   = 1;
    int mStartLayerNumber = -1;
    int mStartFrameNumber = 0;
    int mLastFrameNumber = -1;

    // is used to move layers, don't use this to get mousePos;
    int mMouseMoveY = 0;
    int mPrevFrame = 0;
    int mFrameOffset = 0;
    int mLayerOffset = 0;
    Qt::MouseButton primaryButton = Qt::NoButton;

    bool mScrollingVertically = false;

    bool mCanMoveFrame   = false;
    bool mMovingFrames   = false;

    bool mCanBoxSelect   = false;
    bool mBoxSelecting   = false;

    bool mClickSelecting = false;
    bool mMultiSelectMode = false; // Track if we're in Ctrl+click multi-select mode

    int mFramePosMoveX = 0;
    int mLayerPosMoveY = 0;

    int mMouseMoveX = 0;
    int mMousePressX = 0;

    const static int mOffsetX = 0;
    const static int mOffsetY = 28;
    const static int mLayerDetachThreshold = 5;

    // Inline rename state
    QLineEdit* mInlineEditor = nullptr;
    int mEditingLayerIndex = -1;
    bool mIsInlineEditing = false;

    // Inline cell ID editor state (timeline tracks)
    QLineEdit* mCellInlineEditor = nullptr;
    int mCellEditingLayerIndex = -1;
    int mCellEditingFrame = -1;
    bool mIsCellInlineEditing = false;
    bool mHasAppEventFilter = false;

    // Opacity slider state
    bool mAdjustingOpacity = false;
    int mOpacityLayerIndex = -1;
    static const int mOpacitySliderHeight = 6;

    QVector<LayerRowControls> mLayerRowControls;

    static QHash<int, int> sLayerTagByLayerId;

    static QVector<bool> sLayerTagFilterEnabled;
    static bool sDefaultLayerFilterEnabled;

    static QVector<bool> sLayerTagVisibilityEnabled;
    static bool sDefaultLayerVisibilityEnabled;
    static QHash<int, bool> sLayerVisibilityBackupByLayerId;

    int mLayerInsertSlot = -1;

    // Range marker state
    bool mDraggingRangeStart = false;
    bool mDraggingRangeEnd = false;
    static const int mRangeMarkerWidth = 16;

    QTimer* mSelectionAnimUpdateTimer = nullptr;
    QElapsedTimer mSelectionAnimClock;

    // Scrub throttle — limits scrubTo() calls to ~60 fps so a fast mouse
    // can't flood the canvas repaint pipeline.
    QElapsedTimer mScrubThrottle;

    // New selection logic state
    bool mTempSelectMode = false;   // For auto-deselect on release (Single Click Drag / Alt+Click)
    QPoint mPressPos;               // To calculate drag threshold
    bool mShiftRangeMode = false;   // Active when Shift-dragging
    int mShiftOriginFrame = -1;     // Start of shift interaction
    bool mDragSelectTargetState = true; // State to apply during drag select (Paint Select)
    bool mClickDeselectPending = false; // Deselect all on release only if no drag occurred

    // Multi-Layer Selection Helpers
    bool mDeepInteract = false;
    void deepSelectFrame(int frame, bool selected);
    void deepSelectAllFramesAfter(int frame);
    void deepMoveSelectedFrames(int offset);
    void deepDeselectAll();
    bool isDeepSelectMode(Qt::KeyboardModifiers modifiers) const;

    // Coalesce repeated updateContent() calls into a single update() per event-loop tick
    bool mContentUpdateQueued = false;

    // Image drag-and-drop into timeline layer list
    bool mImageDragActive = false;
    int  mImageDropSlot   = -1;  // visual slot (0=top) where images will be inserted

    // Blank-frame click-drag range select
    bool mBlankDragSelecting = false;   // active while dragging on a blank frame
    bool mBlankDragDidMove   = false;   // true once drag threshold exceeded
    int  mBlankDragStartFrame = -1;     // frame where the drag started
    int  mBlankDragEndFrame   = -1;     // frame currently under cursor (same layer)
    int  mBlankDragLayer      = -1;     // layer the drag originated on
    void paintBlankDragSelectOverlay(QPainter& painter) const;

    void handleImageDragEnter(QDragEnterEvent* e);
    void handleImageDragMove(QDragMoveEvent* e);
    void handleImageDrop(QDropEvent* e);
    int  getImageDropSlot(int y) const;
    void importImagesAsLayers(const QList<QUrl>& urls, int insertSlot);

};

#endif // TIMELINECELLS_H

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

#ifndef EDITOR_H
#define EDITOR_H

#include <functional>
#include <memory>
#include <QVector>
#include <QColor>
#include <QObject>
#include <QDateTime>
#include "pencilerror.h"
#include "pencildef.h"
#include "importimageconfig.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
Q_MOC_INCLUDE("colormanager.h")
Q_MOC_INCLUDE("toolmanager.h")
Q_MOC_INCLUDE("layermanager.h")
Q_MOC_INCLUDE("playbackmanager.h")
Q_MOC_INCLUDE("viewmanager.h")
Q_MOC_INCLUDE("preferencemanager.h")
Q_MOC_INCLUDE("selectionmanager.h")
Q_MOC_INCLUDE("soundmanager.h")
Q_MOC_INCLUDE("overlaymanager.h")
Q_MOC_INCLUDE("clipboardmanager.h")
Q_MOC_INCLUDE("undoredomanager.h")
#endif

class QClipboard;
class QTemporaryDir;
class Object;
class KeyFrame;
class BitmapImage;
class VectorImage;
class LayerCamera;
class MainWindow2;
class BaseManager;
class ColorManager;
class ToolManager;
class LayerManager;
class PlaybackManager;
class ViewManager;
class PreferenceManager;
class SelectionManager;
class SoundManager;
class OverlayManager;
class ShiftTraceManager;
class ClipboardManager;
class UndoRedoManager;
class ScribbleArea;
class TimeLine;
class UndoRedoCommand;
class ActiveFramePool;
class Layer;

enum class SETTING;
class QTimer;

class Editor : public QObject
{
    Q_OBJECT

        Q_PROPERTY(ColorManager*    color    READ color)
        Q_PROPERTY(ToolManager*     tools    READ tools)
        Q_PROPERTY(LayerManager*    layers   READ layers)
        Q_PROPERTY(PlaybackManager* playback READ playback)
        Q_PROPERTY(ViewManager*     view     READ view)
        Q_PROPERTY(PreferenceManager* preference READ preference)
        Q_PROPERTY(SoundManager*    sound    READ sound)
        Q_PROPERTY(SelectionManager* select  READ select)
        Q_PROPERTY(OverlayManager*  overlays READ overlays)
        Q_PROPERTY(ShiftTraceManager*  shiftTrace READ shiftTrace)
        Q_PROPERTY(ClipboardManager* clipboards READ clipboards)
        Q_PROPERTY(UndoRedoManager* undoRedo READ undoRedo)


public:
    explicit Editor(QObject* parent = nullptr);
    ~Editor() override;

    bool init();

    /************************************************************************/
    /* Managers                                                             */
    /************************************************************************/
    ColorManager*      color() const { return mColorManager; }
    ToolManager*       tools() const { return mToolManager; }
    LayerManager*      layers() const { return mLayerManager; }
    PlaybackManager*   playback() const { return mPlaybackManager; }
    ViewManager*       view() const { return mViewManager; }
    PreferenceManager* preference() const { return mPreferenceManager; }
    SoundManager*      sound() const { return mSoundManager; }
    SelectionManager*  select() const { return mSelectionManager; }
    OverlayManager*    overlays() const { return mOverlayManager; }
    ShiftTraceManager* shiftTrace() const { return mShiftTraceManager; }
    ClipboardManager*  clipboards() const { return mClipboardManager; }
    UndoRedoManager*     undoRedo() const { return mUndoRedoManager; }

    Object* object() const { return mObject.get(); }
    std::unique_ptr<Object> takeObject();
    Status openObject(const QString& strFilePath, const std::function<void(int)>& progressChanged, const std::function<void(int)>& progressRangeChanged);
    Status setObject(Object* object);
    void updateObject();
    void prepareSave();

    void setScribbleArea(ScribbleArea* pScirbbleArea) { mScribbleArea = pScirbbleArea; }
    ScribbleArea* getScribbleArea() { return mScribbleArea; }

    int currentFrame() const;
    int fps();
    void setFps(int fps);

    int  currentLayerIndex() const { return mCurrentLayerIndex; }
    void setCurrentLayerIndex(int i);

    void scrubTo(int frameNumber);

    /** True while the user is actively dragging the timeline scrubber.
     *  Set by TimeLine; read by ScribbleArea to throttle canvas repaints. */
    bool isTimelineScrubbing() const { return mTimelineScrubbing; }
    void setTimelineScrubbing(bool s);

    /**
     * @brief The visibility value should match any of the VISIBILITY enum values
     */
    void setLayerVisibility(LayerVisibility visibility);
    LayerVisibility layerVisibility();

    qreal viewScaleInversed();
    void deselectAll() const;
    void selectAll() const;

    // Auto Line Fill support
    bool autoLineFillEnabled() const;
    const QVector<QRgb>& autoLineColors() const;
    void setAutoLineFillEnabled(bool enabled);
    void setAutoLineColors(const QVector<QRgb>& colors);

    // Ignore-in-fill support
    QRgb ignoreFillColor() const;
    void setIgnoreFillColor(QRgb color);
    bool ignoreFillEnabled() const;
    void setIgnoreFillEnabled(bool enabled);

    void clipboardChanged();

signals:

    /** This should be emitted after scrubbing */
    void scrubbed(int frameNumber);

    /** This should be emitted after modifying the frame content */
    void frameModified(int frameNumber);

    /** This should be emitted after modifying multiple frames */
    void framesModified();
    void selectedFramesChanged();

    void updateTimeLine() const;
    void updateTimeLineCached();
    void updateLayerCount();

    void objectLoaded();

    void fpsChanged(int fps);

    void needSave();
    void needDisplayInfo(const QString& title, const QString& body);
    void needDisplayInfoNoTitle(const QString& body);

    void canCopyChanged(bool enabled);
    void canPasteChanged(bool enabled);

    // Emitted when the Ignore In Fill settings change so UI can stay in sync
    void ignoreFillColorChanged(QRgb color);
    void ignoreFillEnabledChanged(bool enabled);

public: //slots

    /** Will call update() and update the canvas
     * Only call this directly If you need the cache to be intact and require the frame to be repainted
    */
    void updateFrame();

    void setModified(int layerNumber, int frameNumber);

    void clearCurrentFrame();

    Status importImage(const QString& filePath, ImportImageConfig importConfig);
    Status importAnimatedImage(const QString& filePath, int frameSpacing, const std::function<void (int)>& progressChanged, const std::function<bool ()>& wasCanceled);

    void scrubNextKeyFrame();
    void scrubPreviousKeyFrame();
    void scrubForward();
    void scrubBackward();

    /**
     * Attempts to create a new keyframe at the current frame and layer. If the current frame already holds a keyframe,
     * the new one will instead be created on the first empty frame that follows. If the current layer is not visible, a
     * warning dialog will be shown to the user and no new keyframe is created.
     * @return The newly created keyframe or `nullptr` if the layer is not visible
     */
    KeyFrame* addNewKey();
    /**
     * Attempts to create a new keyframe at the given position and layer. If a keyframe already exists at the position,
     * the new one will instead be created on the first empty frame that follows. If the layer is not visible, a warning
     * dialog will be shown to the user and no new keyframe is created.
     * @param layerNumber The number of an existing layer on which the keyframe is to be created
     * @param frameIndex The desired position of the new keyframe
     * @return The newly created keyframe or `nullptr` if the layer is not visible
     * @see Editor::addNewKey()
     */
    KeyFrame* addKeyFrame(int layerNumber, int frameIndex);
    void removeKey();

    void switchVisibilityOfLayer(int layerNumber);
    void swapLayers(int i, int j);
    bool canSwapLayers(int layerIndexLeft, int layerIndexRight) const;

    // Legacy backup wrappers (compatibility): forward to UndoRedoManager-based recording.
    void backup(const QString& undoText);
    bool backup(int layerNumber, int frameNumber, const QString& undoText);

    void onCurrentLayerWillChange(int index);

    void copy();
    void copyAndCut();
    void pasteFromPreviousFrame();
    void paste();

    bool canCopy() const;
    bool canPaste() const;

    void increaseLayerVisibilityIndex();
    void decreaseLayerVisibilityIndex();
    void flipSelection(bool flipVertical);
    void repositionImage(QPoint transform, int frame);

    void clearTemporary();
    void addTemporaryDir(QTemporaryDir* dir);

    void settingUpdated(SETTING);

    // Autosave: dirty tracking and timer control
    void markDocumentDirty();
    bool isDocumentDirty() const { return mDocumentDirty; }
    void markDocumentClean();
    void restartAutosaveTimer();
    QTimer* autosaveTimer() const { return mAutosaveTimer; }

    // Legacy API kept for call-site compatibility
    void dontAskAutoSave(bool) {}
    bool autoSaveNeverAskAgain() const { return false; }
    void resetAutoSaveCounter();
    
    // Update undo stack context based on current frame/layer (per-cell vs global)
    void updateUndoContext();

    // Called when the active inner layer within a cell changes (e.g. from CosmicLayersPane)
    void onActiveInnerLayerChanged(int cellId, int newIndex);

private:
    Status importBitmapImage(const QString&, const QTransform& importTransform);
    Status importVectorImage(const QString&);

    void pasteToCanvas(BitmapImage* bitmapImage, int frameNumber);
    void pasteToCanvas(VectorImage* vectorImage, int frameNumber);
    void pasteToFrames();

    bool canCopyBitmapImage(BitmapImage* bitmapImage) const;
    bool canCopyFrames(const Layer* layer) const;
    bool canCopyVectorImage(const VectorImage* vectorImage) const;

    // the object to be edited by the editor
    std::unique_ptr<Object> mObject;

    int mFrame = 1; // current frame number.
    int mCurrentLayerIndex = 0; // the current layer to be edited/displayed

    bool mTimelineScrubbing = false; // true while user drags the timeline scrubber
    QTimer* mScrubDeferTimer = nullptr; // defers updateActiveFrames/updateUndoContext during scrub

    ScribbleArea* mScribbleArea = nullptr;

    ColorManager*      mColorManager = nullptr;
    ToolManager*       mToolManager = nullptr;
    LayerManager*      mLayerManager = nullptr;
    PlaybackManager*   mPlaybackManager = nullptr;
    ViewManager*       mViewManager = nullptr;
    PreferenceManager* mPreferenceManager = nullptr;
    SoundManager*      mSoundManager = nullptr;
    SelectionManager*  mSelectionManager = nullptr;
    OverlayManager*    mOverlayManager = nullptr;
    ShiftTraceManager* mShiftTraceManager = nullptr;
    ClipboardManager*  mClipboardManager = nullptr;
    UndoRedoManager*     mUndoRedoManager = nullptr;

    std::vector< BaseManager* > mAllManagers;

    bool mIsAutosave = true;
    int mAutosaveIntervalMinutes = 15;
    bool mDocumentDirty = false;          // true once any undo-stack change occurs after last save
    QTimer* mAutosaveTimer = nullptr;     // fires every N minutes
    QDateTime mLastSaveTime;              // cooldown guard

    // Legacy fields kept for API compatibility (no longer drive saves)
    int mAutosaveNumber = 15;
    int mAutosaveCounter = 0;
    bool mAutosaveNeverAskAgain = false;

    // Auto Line Fill state
    bool mAutoLineFillEnabled = false;
    QVector<QRgb> mAutoLineColors;
    QRgb mIgnoreFillColor = 0;
    bool mIgnoreFillEnabled = true;

    // Debounced persistence for auto-line settings so QSettings writes do not
    // block the UI on every toggle.
    void scheduleAutoLineSettingsSave();
    QTimer* mAutoLineSettingsSaver = nullptr;

    void makeConnections();

    QList<QTemporaryDir*> mTemporaryDirs;
};

#endif

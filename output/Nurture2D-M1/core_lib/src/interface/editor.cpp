
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

#include "editor.h"

#include <QTimer>
#include <QDateTime>
#include <QImageReader>
#include <QDropEvent>
#include <QTemporaryDir>
#include <QPainter>
#include <QPainterPath>

#include "object.h"
#include "vectorimage.h"
#include "bitmapimage.h"
#include "soundclip.h"
#include "camera.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "structure/celllayerstack.h"
#include "layervector.h"
#include "layercamera.h"
#include "undoredocommand.h"
#include "timelinecommands.h"

#include "colormanager.h"
#include "filemanager.h"
#include "toolmanager.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "viewmanager.h"
#include "preferencemanager.h"
#include "soundmanager.h"
#include "selectionmanager.h"
#include "overlaymanager.h"
#include "shifttracemanager.h"
#include "clipboardmanager.h"
#include "undoredomanager.h"

namespace {
bool hasNavigableKeyframeContent(const Layer* layer, const KeyFrame* keyframe)
{
    if (!layer || !keyframe) {
        return false;
    }

    // Bitmap timeline cells are the numbered drawings. Empty bitmap exposures
    // can still have a keyframe entry, so only navigate to cells that exist.
    if (layer->type() == Layer::BITMAP) {
        const auto* bitmapLayer = dynamic_cast<const LayerBitmap*>(layer);
        const auto* bitmapKeyframe = dynamic_cast<const BitmapKeyFrame*>(keyframe);
        return bitmapLayer && bitmapKeyframe && bitmapKeyframe->cellId() > 0;
    }

    return true;
}
}

#include "scribblearea.h"
#include <QColor>
#include <QSettings>
#include <QTimer>

Editor::Editor(QObject* parent) : QObject(parent)
{
}

std::unique_ptr<Object> Editor::takeObject()
{
    return std::move(mObject);
}

Editor::~Editor()
{
    // a lot more probably needs to be cleaned here...
    clearTemporary();
}

bool Editor::init()
{
    // Initialize managers
    mColorManager = new ColorManager(this);
    mLayerManager = new LayerManager(this);
    mToolManager = new ToolManager(this);
    mPlaybackManager = new PlaybackManager(this);
    mViewManager = new ViewManager(this);
    mPreferenceManager = new PreferenceManager(this);
    mSoundManager = new SoundManager(this);
    mSelectionManager = new SelectionManager(this);
    mOverlayManager = new OverlayManager(this);
    mShiftTraceManager = new ShiftTraceManager(this);
    mClipboardManager = new ClipboardManager(this);
    mUndoRedoManager = new UndoRedoManager(this);

    mAllManagers =
    {
        mColorManager,
        mToolManager,
        mLayerManager,
        mPlaybackManager,
        mViewManager,
        mPreferenceManager,
        mSoundManager,
        mSelectionManager,
        mOverlayManager,
        mShiftTraceManager,
        mClipboardManager,
        mUndoRedoManager
    };

    for (BaseManager* pManager : mAllManagers)
    {
        pManager->init();
    }

    // Default Ignore In Fill so users can immediately fill behind black lineart
    // without opening any options panels. Use pure black and enable it.
    mIgnoreFillColor = qPremultiply(qRgba(0, 0, 0, 255));
    mIgnoreFillEnabled = true;

    // Initialize Auto Line Fill defaults / persisted state.
    {
        QSettings settings(PENCIL2D, PENCIL2D);

        // Master enabled flag: default ON when there's no saved value.
        if (settings.contains("AutoLine/Enabled")) {
            mAutoLineFillEnabled = settings.value("AutoLine/Enabled").toBool();
        } else {
            mAutoLineFillEnabled = true;
            settings.setValue("AutoLine/Enabled", mAutoLineFillEnabled);
        }

        // Colors: default to the 5 standard auto-line colors if nothing saved.
        QVariantList stored = settings.value("AutoLine/Colors").toList();
        mAutoLineColors.clear();
        if (!stored.isEmpty()) {
            for (const QVariant& v : stored) {
                bool ok = false;
                quint32 raw = v.toUInt(&ok);
                if (ok) {
                    mAutoLineColors.append(static_cast<QRgb>(raw));
                }
            }
        }

        if (mAutoLineColors.isEmpty()) {
            QVector<QColor> defaults = { Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::magenta };
            for (const QColor& c : defaults) {
                mAutoLineColors.append(qPremultiply(c.rgba()));
            }

            QVariantList list;
            for (QRgb c : mAutoLineColors) {
                list << static_cast<quint32>(c);
            }
            settings.setValue("AutoLine/Colors", list);
        }
    }

    makeConnections();

    mIsAutosave = mPreferenceManager->isOn(SETTING::AUTO_SAVE);
    mAutosaveNumber = mPreferenceManager->getInt(SETTING::AUTO_SAVE_NUMBER);
    mAutosaveIntervalMinutes = mPreferenceManager->getInt(SETTING::AUTO_SAVE_INTERVAL_MINUTES);

    // Set up the time-based autosave timer
    mAutosaveTimer = new QTimer(this);
    mAutosaveTimer->setSingleShot(false);
    connect(mAutosaveTimer, &QTimer::timeout, this, [this]() {
        if (!mIsAutosave) return;

        // Cooldown: don't fire again within 60 seconds of the last save
        if (mLastSaveTime.isValid() && mLastSaveTime.secsTo(QDateTime::currentDateTime()) < 60) return;

        // Fire if the document has unsaved changes, OR if it has never been saved
        // (no file path = always worth prompting, even with no strokes drawn)
        const bool hasNoPath = !mObject || mObject->filePath().isEmpty();
        if (!mDocumentDirty && !hasNoPath) return;

        emit needSave();
    });
    if (mIsAutosave) {
        mAutosaveTimer->start(mAutosaveIntervalMinutes * 60 * 1000);
    }

    return true;
}

void Editor::scheduleAutoLineSettingsSave()
{
    if (!mAutoLineSettingsSaver) {
        mAutoLineSettingsSaver = new QTimer(this);
        mAutoLineSettingsSaver->setSingleShot(true);
        // Small delay to coalesce rapid toggles into one disk write while
        // keeping persistence responsive.
        mAutoLineSettingsSaver->setInterval(150);
        connect(mAutoLineSettingsSaver, &QTimer::timeout, this, [this]() {
            QSettings settings(PENCIL2D, PENCIL2D);
            settings.setValue("AutoLine/Enabled", mAutoLineFillEnabled);

            QVariantList list;
            list.reserve(mAutoLineColors.size());
            for (QRgb c : mAutoLineColors) {
                list << static_cast<quint32>(c);
            }
            settings.setValue("AutoLine/Colors", list);
        });
    }

    mAutoLineSettingsSaver->start();
}

int Editor::currentFrame() const
{
    return mFrame;
}

int Editor::fps()
{
    return mPlaybackManager->fps();
}

void Editor::setFps(int fps)
{
    mPreferenceManager->set(SETTING::FPS, fps);
    emit fpsChanged(fps);
}

void Editor::makeConnections()
{
    connect(mPreferenceManager, &PreferenceManager::optionChanged, this, &Editor::settingUpdated);
    connect(mUndoRedoManager, &UndoRedoManager::didUpdateUndoStack, this, &Editor::markDocumentDirty);
    connect(mPreferenceManager, &PreferenceManager::optionChanged, mUndoRedoManager, &UndoRedoManager::onSettingChanged);

    // XXX: This is a hack to prevent crashes until #864 is done (see #1412)
    // Legacy signal for sanitizing legacy backups removed.
    connect(mLayerManager, &LayerManager::currentLayerWillChange, this, &Editor::onCurrentLayerWillChange);
}

void Editor::settingUpdated(SETTING setting)
{
    switch (setting)
    {
    case SETTING::AUTO_SAVE:
        mIsAutosave = mPreferenceManager->isOn(SETTING::AUTO_SAVE);
        if (mAutosaveTimer) {
            if (mIsAutosave) {
                mAutosaveTimer->start(mAutosaveIntervalMinutes * 60 * 1000);
            } else {
                mAutosaveTimer->stop();
            }
        }
        break;
    case SETTING::AUTO_SAVE_NUMBER:
        mAutosaveNumber = mPreferenceManager->getInt(SETTING::AUTO_SAVE_NUMBER);
        break;
    case SETTING::AUTO_SAVE_INTERVAL_MINUTES:
        mAutosaveIntervalMinutes = mPreferenceManager->getInt(SETTING::AUTO_SAVE_INTERVAL_MINUTES);
        if (mAutosaveTimer && mIsAutosave) {
            mAutosaveTimer->start(mAutosaveIntervalMinutes * 60 * 1000);
        }
        break;
    case SETTING::ONION_TYPE:
        mScribbleArea->onOnionSkinTypeChanged();
        emit updateTimeLineCached();
        break;
    case SETTING::FRAME_POOL_SIZE:
        mObject->setActiveFramePoolSize(mPreferenceManager->getInt(SETTING::FRAME_POOL_SIZE));
        break;
    case SETTING::LAYER_VISIBILITY:
        mScribbleArea->setLayerVisibility(static_cast<LayerVisibility>(mPreferenceManager->getInt(SETTING::LAYER_VISIBILITY)));
        emit updateTimeLine();
        break;
    default:
        break;
    }
}

void Editor::onCurrentLayerWillChange(int index)
{
    Layer* newLayer = layers()->getLayer(index);
    Layer* currentLayer = layers()->currentLayer();
    Q_ASSERT(newLayer && currentLayer);
    if (currentLayer->type() != newLayer->type()) {
        // We apply transform changes upon leaving a layer and deselect all
        mScribbleArea->applyTransformedSelection();

        if (currentLayer->type() == Layer::VECTOR) {
            auto keyFrame = static_cast<VectorImage*>(currentLayer->getLastKeyFrameAtPosition(mFrame));
            if (keyFrame)
            {
                keyFrame->deselectAll();
            }
        }

        select()->resetSelectionProperties();
    }
}

void Editor::markDocumentDirty()
{
    mDocumentDirty = true;
}

void Editor::markDocumentClean()
{
    mDocumentDirty = false;
    mLastSaveTime = QDateTime::currentDateTime();
    // Restart the timer so the next autosave fires N minutes from now, not from
    // whenever the timer last ticked.
    if (mAutosaveTimer && mIsAutosave) {
        mAutosaveTimer->start(mAutosaveIntervalMinutes * 60 * 1000);
    }
}

void Editor::resetAutoSaveCounter()
{
    // Legacy stub — timer-based system doesn't use a counter.
    // markDocumentClean() is called by MainWindow2 after a successful save.
}

void Editor::restartAutosaveTimer()
{
    if (mAutosaveTimer && mIsAutosave) {
        mAutosaveTimer->start(mAutosaveIntervalMinutes * 60 * 1000);
    }
}

void Editor::copy()
{
    Layer* currentLayer = layers()->currentLayer();

    Q_ASSERT(currentLayer != nullptr);

    if (!canCopy()) { return; }
    if (currentLayer->hasAnySelectedFrames() && !select()->somethingSelected()) {
        clipboards()->copySelectedFrames(currentLayer);
    } else if (currentLayer->type() == Layer::BITMAP) {
        auto selMan = select();
        // Check compatibility before cast?
        // Note: currentFrame does return int.
        BitmapImage* bitmapImage = static_cast<LayerBitmap*>(currentLayer)->getBitmapImageAtFrame(currentFrame());
        if (selMan && !selMan->selectionPathRaw().isEmpty() && bitmapImage && bitmapImage->image() && !bitmapImage->image()->isNull()) {
            QPainterPath path = selMan->selectionPathWithOffsets();
            QRect bbox = path.boundingRect().toAlignedRect();
            if (!bbox.isEmpty()) {
                QImage mask(bbox.size(), QImage::Format_ARGB32_Premultiplied);
                mask.fill(Qt::transparent);
                {
                    QPainter maskPainter(&mask);
                    QPainterPath local = path;
                    local.translate(-bbox.topLeft());
                    maskPainter.fillPath(local, Qt::white);
                }

                QPoint srcTopLeft = bbox.topLeft() - bitmapImage->topLeft();
                QImage sub = bitmapImage->image()->copy(QRect(srcTopLeft, bbox.size()));
                QImage masked(sub.size(), QImage::Format_ARGB32_Premultiplied);
                masked.fill(Qt::transparent);
                {
                    QPainter p(&masked);
                    p.drawImage(QPoint(0,0), sub);
                    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                    p.drawImage(QPoint(0,0), mask);
                }
                QVector<BitmapImage::HybridVectorStroke> hybridSegments;
                bitmapImage->collectHybridStrokesWithMask(mask, bbox.topLeft(), &hybridSegments);
                if (!hybridSegments.isEmpty()) {
                    clipboards()->setBitmapFromImageAtWithHybrid(masked, bbox.topLeft(), hybridSegments);
                } else {
                    clipboards()->setBitmapFromImageAt(masked, bbox.topLeft());
                }
            }
        } else {
            clipboards()->copyBitmapImage(bitmapImage, selMan ? selMan->mySelectionRect() : QRectF());
        }
    } else if (currentLayer->type() == Layer::VECTOR) {
        VectorImage* vectorImage = static_cast<VectorImage*>(currentLayer->getLastKeyFrameAtPosition(currentFrame()));
        clipboards()->copyVectorImage(vectorImage);
    }
}

void Editor::copyAndCut()
{
    // Guard against nested operations while a floating selection is active
    if (!canCopy()) { return; }
    copy();

    Layer* currentLayer = layers()->currentLayer();

    if (currentLayer->hasAnySelectedFrames() && !select()->somethingSelected()) {
        for (int pos : currentLayer->selectedKeyFramesPositions()) {
            currentLayer->removeKeyFrame(pos);
        }
        emit layers()->currentLayerChanged(currentLayerIndex());
        emit updateTimeLine();
        return;
    }

    if (currentLayer->type() == Layer::BITMAP || currentLayer->type() == Layer::VECTOR) {
        mScribbleArea->deleteSelection();
        deselectAll();
    }
}

void Editor::pasteFromPreviousFrame()
{
    Layer* currentLayer = layers()->currentLayer();
    int prevFrame = currentLayer->getPreviousKeyFramePosition(mFrame);
    if (!currentLayer->keyExists(mFrame) || prevFrame == mFrame)
    {
        return;
    }

    if (currentLayer->type() == Layer::BITMAP)
    {
        const UndoSaveState* saveState_pastePrev = mUndoRedoManager->state(UndoRedoRecordType::KEYFRAME_MODIFY);
        BitmapImage* bitmapImage = static_cast<LayerBitmap*>(currentLayer)->getBitmapImageAtFrame(prevFrame);
        if (select()->somethingSelected())
        {
            BitmapImage copy = bitmapImage->copy(select()->mySelectionRect().toRect());
            pasteToCanvas(&copy, mFrame);
        }
        else
        {
            pasteToCanvas(bitmapImage, mFrame);
        }
        if (saveState_pastePrev && mUndoRedoManager) mUndoRedoManager->record(saveState_pastePrev, tr("Paste from Previous Keyframe"));
    }
    else if (currentLayer->type() == Layer::VECTOR)
    {
        const UndoSaveState* saveState_pastePrevV = mUndoRedoManager->state(UndoRedoRecordType::KEYFRAME_MODIFY);
        VectorImage* vectorImage = static_cast<VectorImage*>(currentLayer->getKeyFrameAt(prevFrame));
        pasteToCanvas(vectorImage, mFrame);
        if (saveState_pastePrevV && mUndoRedoManager) mUndoRedoManager->record(saveState_pastePrevV, tr("Paste from Previous Keyframe"));
    }
}

void Editor::pasteToCanvas(BitmapImage* bitmapImage, int frameNumber)
{
    Layer* currentLayer = layers()->currentLayer();

    Q_ASSERT(currentLayer->type() == Layer::BITMAP);

    if (select()->somethingSelected())
    {
       QRectF selection = select()->mySelectionRect();
       if (bitmapImage->width() <= selection.width() && bitmapImage->height() <= selection.height())
       {
           bitmapImage->moveTopLeft(selection.topLeft());
       }
       else
       {
           bitmapImage->transform(selection, true);
       }
    }
    mScribbleArea->handleDrawingOnEmptyFrame();

    // Task 8.1 (Req 8.1): Paste targets the Active_Inner_Layer, not the composite.
    // Retrieve the active inner layer image via CellBank so the paste writes only
    // to the currently selected inner layer and leaves all other inner layers intact.
    auto bitmapLayer = static_cast<LayerBitmap*>(currentLayer);
    KeyFrame* kf = bitmapLayer->keyExists(frameNumber)
                   ? bitmapLayer->getKeyFrameAt(frameNumber)
                   : bitmapLayer->getLastKeyFrameAtPosition(frameNumber);
    BitmapImage* canvasImage = nullptr;
    CellLayerStack* stack = nullptr;
    if (kf) {
        BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
        CellBank* bank = bitmapLayer->cellBank();
        canvasImage = bank->getActiveInnerLayerImage(bkf->cellId());
        stack = bank->getCellLayerStack(bkf->cellId());
    }
    if (!canvasImage) {
        // Fallback to composite (legacy path) if inner layer API unavailable
        canvasImage = bitmapLayer->getBitmapImageAtFrame(frameNumber);
    }

    // Paste clipboard onto the active inner layer
    canvasImage->paste(bitmapImage);

    // Invalidate composite so CanvasPainter recomputes it on next render
    if (stack) stack->invalidateComposite();

    // TODO: currently we don't support placing an image without already pasting it on an already existing
    // image, this should be reworked such that a hovering selection could be shown, before applying it...
    select()->setSelection(bitmapImage->bounds());
    emit frameModified(frameNumber);
}

void Editor::pasteToCanvas(VectorImage* vectorImage, int frameNumber)
{
    Layer* currentLayer = layers()->currentLayer();

    Q_ASSERT(currentLayer->type() == Layer::VECTOR);

    deselectAll();
    mScribbleArea->handleDrawingOnEmptyFrame();
    VectorImage* canvasImage = static_cast<VectorImage*>(currentLayer->getLastKeyFrameAtPosition(frameNumber));
    canvasImage->paste(*vectorImage);
    select()->setSelection(vectorImage->getSelectionRect());
    emit frameModified(frameNumber);
}

void Editor::pasteToFrames()
{
    auto clipboardFrames = clipboards()->getClipboardFrames();
    Q_ASSERT(!clipboardFrames.empty());
    Layer* currentLayer = layers()->currentLayer();

    currentLayer->deselectAll();

    int newPositionOffset = mFrame - clipboardFrames.cbegin()->first;
    for (auto it = clipboardFrames.cbegin(); it != clipboardFrames.cend(); ++it)
    {
        int newPosition = it->first + newPositionOffset;

        KeyFrame* keyFrameNewPos = currentLayer->getKeyFrameWhichCovers(newPosition);

        if (keyFrameNewPos != nullptr) {

            // Select and move any frames that may come into contact with the new position
            currentLayer->newSelectionOfConnectedFrames(newPosition);
            currentLayer->moveSelectedFrames(1);
        }

        KeyFrame* key = it->second;
        // It's a bug if the keyframe is nullptr at this point...
        Q_ASSERT(key != nullptr);

        // TODO: undo/redo implementation
        currentLayer->addKeyFrame(newPosition, key);
        if (currentLayer->type() == Layer::SOUND)
        {
            auto soundClip = static_cast<SoundClip*>(key);
            sound()->loadSound(soundClip, soundClip->fileName());
        }

        currentLayer->setFrameSelected(key->pos(), true);
    }
    updateUndoContext();
}

void Editor::paste()
{
    Layer* currentLayer = layers()->currentLayer();

    Q_ASSERT(currentLayer != nullptr);

    // Do not start a paste while a floating selection is already active or
    // needs committing. The user should commit or cancel the current
    // temporary buffer first to keep undo/redo and clipboard state coherent.
    if (!canPaste()) { return; }

    if (clipboards()->framesIsEmpty()) {

        clipboards()->setFromSystemClipboard(mScribbleArea->getCentralPoint(), currentLayer);

        BitmapImage clipboardImage = clipboards()->getBitmapClipboard();
        VectorImage clipboardVectorImage = clipboards()->getVectorClipboard();

        if (currentLayer->type() == Layer::BITMAP && clipboardImage.isLoaded()) {
            // Bitmap paste: route through SelectionManager floating overlay so the
            // user can move/transform the pasted content before committing. We
            // do not touch the underlying bitmap or create an undo record here;
            // commit will be handled by deselectAll()/Enter via
            // SelectionManager::commitFloatingMove() and per-keyframe undo.

            auto selMan = select();
            if (selMan) {
                // If there is a pending floating commit from a previous
                // operation, finalize it first to avoid stacking overlays.
                if (selMan->needsFloatingCommit()) {
                    const UndoSaveState* undoState = undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
                    mScribbleArea->handleDrawingOnEmptyFrame();
                    selMan->commitFloatingMove();
                    setModified(layers()->currentLayerIndex(), currentFrame());
                    if (undoState && undoRedo()) {
                        undoRedo()->record(undoState, tr("Lasso Select"));
                    }
                }

                // Start a new floating selection from the clipboard image at
                // its stored canvas-space origin, including any hybrid
                // strokes that accompany the bitmap content.
                selMan->beginFloatingFromImage(*clipboardImage.image(), clipboardImage.topLeft(),
                                               clipboardImage.hybridStrokes());
            }
        } else if (currentLayer->type() == Layer::VECTOR && !clipboardVectorImage.isEmpty()) {
            // Vector paste still uses the existing immediate paste pipeline and
            // records an undo entry at paste time.
            const UndoSaveState* saveState_paste = mUndoRedoManager->state(UndoRedoRecordType::KEYFRAME_MODIFY);
            pasteToCanvas(&clipboardVectorImage, mFrame);
            if (saveState_paste && mUndoRedoManager) mUndoRedoManager->record(saveState_paste, tr("Paste"));
        }
    } else {
        // TODO: implement undo/redo for multi-frame paste
        pasteToFrames();
    }

    emit frameModified(mFrame);
}

void Editor::flipSelection(bool flipVertical)
{
    const UndoSaveState* saveState_flip = mUndoRedoManager->state(UndoRedoRecordType::KEYFRAME_MODIFY);
    mScribbleArea->flipSelection(flipVertical);
    if (saveState_flip && mUndoRedoManager) mUndoRedoManager->record(saveState_flip, flipVertical ? tr("Flip selection vertically") : tr("Flip selection horizontally"));
}

void Editor::repositionImage(QPoint transform, int frame)
{
    if (layers()->currentLayer()->type() == Layer::BITMAP)
    {
        scrubTo(frame);
        LayerBitmap* layer = static_cast<LayerBitmap*>(layers()->currentLayer());
        QRect reposRect = layer->getFrameBounds(frame);
        select()->setSelection(reposRect);
        QPoint point = reposRect.topLeft();
        point += transform;
        const UndoSaveState* saveState_repos = mUndoRedoManager->state(UndoRedoRecordType::KEYFRAME_MODIFY);
        layer->repositionFrame(point, frame);
        if (saveState_repos && mUndoRedoManager) mUndoRedoManager->record(saveState_repos, tr("Reposition frame")); // TODO: group multiple reposition operations if needed.
    }
}

void Editor::setModified(int layerNumber, int frameNumber)
{
    Layer* layer = object()->getLayer(layerNumber);
    if (layer == nullptr) { return; }

    layer->setModified(frameNumber, true);

    emit frameModified(frameNumber);
}

void Editor::clipboardChanged()
{
    Layer* layer = layers()->currentLayer();

    clipboards()->setFromSystemClipboard(mScribbleArea->getCentralPoint(), layer);

    bool canCopyState = canCopy();
    bool canPasteState = canPaste();

    emit canCopyChanged(canCopyState);
    emit canPasteChanged(canPasteState);
}

void Editor::setLayerVisibility(LayerVisibility visibility) {
    mScribbleArea->setLayerVisibility(visibility);
    emit updateTimeLine();
}

LayerVisibility Editor::layerVisibility()
{
    return mScribbleArea->getLayerVisibility();
}

qreal Editor::viewScaleInversed()
{
    return view()->getScaleInversed();
}

void Editor::increaseLayerVisibilityIndex()
{
    mScribbleArea->increaseLayerVisibilityIndex();
    emit updateTimeLine();
}

void Editor::decreaseLayerVisibilityIndex()
{
    mScribbleArea->decreaseLayerVisibilityIndex();
    emit updateTimeLine();
}

void Editor::addTemporaryDir(QTemporaryDir* const dir)
{
    mTemporaryDirs.append(dir);
}

void Editor::clearTemporary()
{
    while(!mTemporaryDirs.isEmpty())
    {
        QTemporaryDir* t = mTemporaryDirs.takeLast();
        t->remove();
        delete t;
    }
}

Status Editor::openObject(const QString& strFilePath, const std::function<void(int)>& progressChanged, const std::function<void(int)>& progressRangeChanged)
{
    // Check for potential issues with the file
    Q_ASSERT(!strFilePath.isEmpty());
    QFileInfo fileInfo(strFilePath);
    DebugDetails dd;
    dd << QString("Raw file path: %1").arg(strFilePath);
    dd << QString("Resolved file path: %1").arg(fileInfo.absoluteFilePath());
    if (fileInfo.isDir())
    {
        return Status(Status::ERROR_FILE_CANNOT_OPEN,
                      dd,
                      tr("Could not open file"),
                      tr("The file you have selected is a directory, so we are unable to open it. "
                         "If you are are trying to open a project that uses the old structure, "
                         "please open the file ending with .pcl, not the data folder."));
    }
    if (!fileInfo.exists())
    {
        return Status(Status::FILE_NOT_FOUND,
                      dd,
                      tr("Could not open file"),
                      tr("The file you have selected does not exist, so we are unable to open it. "
                         "Please make sure that you've entered the correct path and that the file is accessible and try again."));
    }
    if (!fileInfo.isReadable())
    {
        dd << QString("Permissions: 0x%1").arg(fileInfo.permissions(), 0, 16);
        return Status(Status::ERROR_FILE_CANNOT_OPEN,
                      dd,
                      tr("Could not open file"),
                      tr("This program does not have permission to read the file you have selected. "
                         "Please check that you have read permissions for this file and try again."));
    }

    int progress = 0;
    FileManager fm(this);
    connect(&fm, &FileManager::progressChanged, [&progress, &progressChanged](int p)
    {
        progressChanged(progress = p);
    });
    connect(&fm, &FileManager::progressRangeChanged, [&progressRangeChanged](int max)
    {
        progressRangeChanged(max + 3);
    });

    QString fullPath = fileInfo.absoluteFilePath();

    Object* object = fm.load(fullPath);

    Status fmStatus = fm.error();
    if (!fmStatus.ok())
    {
        dd.collect(fmStatus.details());
        fmStatus.setDetails(dd);
        return fmStatus;
    }

    if (object == nullptr)
    {
        return Status(Status::ERROR_FILE_CANNOT_OPEN,
                      dd,
                      tr("Could not open file"),
                      tr("An unknown error occurred while trying to load the file and we are not able to load your file."));
    }

    setObject(object);

    progressChanged(progress + 1);

    layers()->notifyAnimationLengthChanged();
    setFps(playback()->fps());

    return Status::OK;
}

Status Editor::setObject(Object* newObject)
{
    Q_ASSERT(newObject);

    if (newObject == mObject.get())
    {
        return Status::SAFE;
    }

    mObject.reset(newObject);
    qDebug() << "Editor::setObject::mObject reset";

    updateObject();
    qDebug() << "Editor::setObject::updateObject done";

    // Make sure that object is fully loaded before calling managers.
    for (BaseManager* m : mAllManagers)
    {
        qDebug() << "Editor::setObject::loading manager" << m->metaObject()->className();
        m->load(mObject.get());
    }
    emit objectLoaded();

    return Status::OK;
}

void Editor::updateObject()
{
    qDebug() << "Editor::updateObject::setCurrentLayerIndex";
    setCurrentLayerIndex(mObject->data()->getCurrentLayer());
    qDebug() << "Editor::updateObject::scrubTo";
    scrubTo(mObject->data()->getCurrentFrame());

    mAutosaveCounter = 0;
    mAutosaveNeverAskAgain = false;

    // A new/unsaved project (no file path) is immediately considered dirty
    // so the autosave timer will prompt the user even if they haven't drawn anything.
    mDocumentDirty = !mObject || mObject->filePath().isEmpty();

    if (mPreferenceManager)
    {
        mObject->setActiveFramePoolSize(mPreferenceManager->getInt(SETTING::FRAME_POOL_SIZE));
    }

    emit updateLayerCount();
}

Status Editor::importBitmapImage(const QString& filePath, const QTransform& importTransform)
{
    QImageReader reader(filePath);

    Q_ASSERT(layers()->currentLayer()->type() == Layer::BITMAP);
    const auto layer = static_cast<LayerBitmap*>(layers()->currentLayer());

    if (!layer->visible())
    {
        mScribbleArea->showLayerNotVisibleWarning();
        return Status::SAFE;
    }

    Status status = Status::OK;
    DebugDetails dd;
    dd << QString("Raw file path: %1").arg(filePath);

    QImage img(reader.size(), QImage::Format_ARGB32_Premultiplied);
    if (!reader.read(&img)) {
        QString format = reader.format();
        if (!format.isEmpty())
        {
            dd << QString("QImageReader format: %1").arg(format);
        }
        dd << QString("QImageReader ImageReaderError type: %1").arg(reader.errorString());

        QString errorDesc;
        switch(reader.error())
        {
        case QImageReader::ImageReaderError::FileNotFoundError:
            errorDesc = tr("File not found at path \"%1\". Please check the image is present at the specified location and try again.").arg(filePath);
            break;
        case QImageReader::UnsupportedFormatError:
            errorDesc = tr("Image format is not supported. Please convert the image file to one of the following formats and try again:\n%1")
                        .arg(QString::fromUtf8(reader.supportedImageFormats().join(", ")));
            break;
        default:
            errorDesc = tr("An error has occurred while reading the image. Please check that the file is a valid image and try again.");
        }

        status = Status(Status::FAIL, dd, tr("Import failed"), errorDesc);
    }

    const QPoint pos = importTransform.map(QPoint(-img.width() / 2,
                                        -img.height() / 2));

    if (!layer->keyExists(mFrame))
    {
        const bool ok = addNewKey();
        Q_ASSERT(ok);
    }
    const UndoSaveState* saveState_importBitmap = mUndoRedoManager->state(UndoRedoRecordType::KEYFRAME_MODIFY);
    BitmapImage* bitmapImage = layer->getBitmapImageAtFrame(mFrame);
    BitmapImage importedBitmapImage(pos, img);
    bitmapImage->paste(&importedBitmapImage);
    emit frameModified(bitmapImage->pos());

    // Record undo while still on the frame we modified, so redo captures the correct bitmap
    if (saveState_importBitmap && mUndoRedoManager) mUndoRedoManager->record(saveState_importBitmap, tr("Import Image"));

    // Advance playhead after recording
    scrubTo(mFrame+1);

    return status;
}

Status Editor::importVectorImage(const QString& filePath)
{
    Q_ASSERT(layers()->currentLayer()->type() == Layer::VECTOR);

    auto layer = static_cast<LayerVector*>(layers()->currentLayer());

    Status status = Status::OK;
    DebugDetails dd;
    dd << QString("Raw file path: %1").arg(filePath);

    VectorImage* vectorImage = layer->getVectorImageAtFrame(currentFrame());
    if (vectorImage == nullptr)
    {
        addNewKey();
        vectorImage = layer->getVectorImageAtFrame(currentFrame());
    }

    VectorImage importedVectorImage;
    bool ok = importedVectorImage.read(filePath);
    if (ok)
    {
        const UndoSaveState* saveState_importVector = mUndoRedoManager->state(UndoRedoRecordType::KEYFRAME_MODIFY);
        importedVectorImage.selectAll();
        vectorImage->paste(importedVectorImage);
        emit frameModified(importedVectorImage.pos());

        if (saveState_importVector && mUndoRedoManager) mUndoRedoManager->record(saveState_importVector, tr("Import Image"));
    }
    else {
        status = Status(Status::FAIL, dd, tr("Import failed"), tr("You cannot import images into a vector layer."));
    }

    return status;
}

Status Editor::importImage(const QString& filePath, const ImportImageConfig importConfig)
{
    Layer* layer = layers()->currentLayer();

    DebugDetails dd;
    dd << QString("Raw file path: %1").arg(filePath);

    QTransform transform;
    switch (importConfig.positionType)
    {
        case ImportImageConfig::CenterOfCamera: {
            LayerCamera* layerCam = static_cast<LayerCamera*>(layers()->getCameraLayerBelow(currentLayerIndex()));
            Q_ASSERT(layerCam);
            transform = layerCam->getViewAtFrame(importConfig.importFrame).inverted();
            break;
        }
        case ImportImageConfig::CenterOfCameraFollowed: {
            LayerCamera* camera = static_cast<LayerCamera*>(layers()->getCameraLayerBelow(currentLayerIndex()));
            Q_ASSERT(camera);
            transform = camera->getViewAtFrame(currentFrame()).inverted();
            break;
        }
        case ImportImageConfig::CenterOfView: {
            QPointF centralPoint = mScribbleArea->getCentralPoint();
            transform = QTransform::fromTranslate(centralPoint.x(), centralPoint.y());
            break;
        }
        case ImportImageConfig::CenterOfCanvas:
        case ImportImageConfig::None: {
            transform = QTransform();
            break;
        }
    }

    switch (layer->type())
    {
    case Layer::BITMAP:
        return importBitmapImage(filePath, transform);

    case Layer::VECTOR:
        return importVectorImage(filePath);

    default:
        dd << QString("Current layer: %1").arg(layer->type());
        return Status(Status::ERROR_INVALID_LAYER_TYPE, dd, tr("Import failed"), tr("You can only import images to a bitmap layer."));
    }
}

Status Editor::importAnimatedImage(const QString& filePath, int frameSpacing, const std::function<void(int)>& progressChanged, const std::function<bool()>& wasCanceled)
{
    frameSpacing = qMax(1, frameSpacing);

    DebugDetails dd;
    dd << QString("Raw file path: %1").arg(filePath);

    Layer* layer = layers()->currentLayer();
    if (layer->type() != Layer::BITMAP)
    {
        dd << QString("Current layer: %1").arg(layer->type());
        return Status(Status::ERROR_INVALID_LAYER_TYPE, dd, tr("Import failed"), tr("You can only import images to a bitmap layer."));
    }
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layers()->currentLayer());

    QImageReader reader(filePath);
    dd << QString("QImageReader format: %1").arg(QString(reader.format()));
    if (!reader.supportsAnimation()) {
        return Status(Status::ERROR_INVALID_LAYER_TYPE, dd, tr("Import failed"), tr("The selected image has a format that does not support animation."));
    }

    QImage img(reader.size(), QImage::Format_ARGB32_Premultiplied);
    const QPoint pos(view()->getView().dx() - (img.width() / 2),
                     view()->getView().dy() - (img.height() / 2));
    int totalFrames = reader.imageCount();
    
    const UndoSaveState* saveState_importAnimated = mUndoRedoManager->state(UndoRedoRecordType::KEYFRAME_MODIFY);
    
    while (reader.read(&img))
    {
        if (reader.error())
        {
            dd << QString("QImageReader ImageReaderError type: %1").arg(reader.errorString());

            QString errorDesc;
            switch(reader.error())
            {
            case QImageReader::ImageReaderError::FileNotFoundError:
                errorDesc = tr("File not found at path \"%1\". Please check the image is present at the specified location and try again.").arg(filePath);
                break;
            case QImageReader::UnsupportedFormatError:
                errorDesc = tr("Image format is not supported. Please convert the image file to one of the following formats and try again:\n%1")
                            .arg((QString)reader.supportedImageFormats().join(", "));
                break;
            default:
                errorDesc = tr("An error has occurred while reading the image. Please check that the file is a valid image and try again.");
            }

            if (saveState_importAnimated && mUndoRedoManager) mUndoRedoManager->record(saveState_importAnimated, tr("Import Animated Image"));
            return Status(Status::FAIL, dd, tr("Import failed"), errorDesc);
        }

        if (!bitmapLayer->keyExists(mFrame))
        {
            addNewKey();
        }
        BitmapImage* bitmapImage = bitmapLayer->getBitmapImageAtFrame(mFrame);
        BitmapImage importedBitmapImage(pos, img);
        bitmapImage->paste(&importedBitmapImage);
        emit frameModified(bitmapImage->pos());

        if (wasCanceled())
        {
            break;
        }

        scrubTo(mFrame + frameSpacing);

        progressChanged(qFloor(qMin(static_cast<double>(reader.currentImageNumber()) / totalFrames, 1.0) * 100));
    }
    
    if (saveState_importAnimated && mUndoRedoManager) mUndoRedoManager->record(saveState_importAnimated, tr("Import Animated Image"));

    return Status::OK;
}

void Editor::selectAll() const
{
    Layer* layer = layers()->currentLayer();

    QRectF rect;
    if (layer->type() == Layer::BITMAP)
    {
        // Selects the drawn area (bigger or smaller than the screen). It may be more accurate to select all this way
        // as the drawing area is not limited
        BitmapImage *bitmapImage = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(mFrame);
        if (bitmapImage == nullptr) { return; }

        rect = bitmapImage->bounds();
    }
    else if (layer->type() == Layer::VECTOR)
    {
        VectorImage *vectorImage = static_cast<VectorImage*>(layer->getLastKeyFrameAtPosition(mFrame));
        if (vectorImage != nullptr)
        {
            vectorImage->selectAll();
            rect = vectorImage->getSelectionRect();
        }
    }
    // Use setSelectionPath with a rectangular path so both the legacy rect-based
    // selection system and the lasso tool's handle/gizmo system (which checks
    // selectionPathRaw().isEmpty()) work correctly after Select All.
    if (!rect.isEmpty()) {
        QPainterPath rectPath;
        rectPath.addRect(rect);
        select()->setSelectionPath(rectPath);
        // Switch to lasso select so the user can immediately manipulate,
        // scale, rotate or delete the selection.
        tools()->setCurrentTool(LASSO_SELECT);
    } else {
        select()->setSelection(rect, false);
    }
}

void Editor::deselectAll() const
{
    // Allow the active tool to handle selection commit/cancel semantics first
    bool handledCommit = false;
    if (tools() && tools()->currentTool()) {
        handledCommit = tools()->currentTool()->handleDeselectAll();
    }

    if (!handledCommit) {
        SelectionManager* selMan = select();
        // If there is a floating selection with an active transform (e.g. pasted
        // content that has been rotated or scaled), bake the transform into the
        // floating buffer before committing so the visual result matches the
        // overlay. This mirrors the Enter-key commit path in LassoSelectTool.
        if (selMan && selMan->hasFloating() && !selMan->selectionTransform().isIdentity()) {
            selMan->bakeTransformIntoFloating();
        }

        // If a non-floating selection transform is active (e.g., after Move),
        // commit it before clearing the selection.
        mScribbleArea->applyTransformedSelection();

        // Additionally, ensure any pending floating move (cut or paste) is
        // committed and recorded as a single undoable action.
        if (selMan && selMan->needsFloatingCommit()) {
            // Capture undo state before committing
            const UndoSaveState* undoState = undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
            selMan->commitFloatingMove();
            const_cast<Editor*>(this)->setModified(layers()->currentLayerIndex(), currentFrame());
            // Record undo after commit
            if (tools() && tools()->currentTool()) {
                undoRedo()->record(undoState, tools()->currentTool()->typeName());
            }
        }
    }
    select()->resetSelectionProperties();

    Layer* layer = layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (layer->type() == Layer::VECTOR)
    {
        VectorImage *vectorImage = static_cast<VectorImage*>(layer->getLastKeyFrameAtPosition(mFrame));
        if (vectorImage != nullptr)
        {
            vectorImage->deselectAll();
        }
    }

    if (layer->hasAnySelectedFrames()) {
        layer->deselectAll();
        emit updateTimeLine();
    }

    // If we were in Move tool, ensure we return to Select tool after deselecting
    if (tools() && tools()->currentTool() && tools()->currentTool()->type() == ToolType::MOVE)
    {
        tools()->setCurrentTool(ToolType::SELECT);
    }
}

void Editor::updateFrame()
{
    if (mScribbleArea) {
        mScribbleArea->updateFrame();
    }
}

void Editor::setCurrentLayerIndex(int i)
{
    mCurrentLayerIndex = i;

    Layer* layer = mObject->getLayer(i);
    for (auto mgr : mAllManagers)
    {
        mgr->workingLayerChanged(layer);
    }
    updateUndoContext();  // Update undo stack based on current layer
}

void Editor::setTimelineScrubbing(bool s)
{
    mTimelineScrubbing = s;
    // When scrubbing ends, immediately flush the deferred work so the
    // undo context and active frame pool are correct before the user draws.
    if (!s && mScrubDeferTimer && mScrubDeferTimer->isActive()) {
        mScrubDeferTimer->stop();
        mObject->updateActiveFrames(mFrame);
        updateUndoContext();
    }
}

void Editor::scrubTo(int frame)
{
    if (frame < 1) { frame = 1; }
    mFrame = frame;

    // FIXME: should not emit Timeline update here.
    // Editor must be an individual class.
    // Will remove all Timeline related code in Editor class.
    if (mPlaybackManager && !mPlaybackManager->isPlaying())
    {
        emit updateTimeLineCached(); // needs to update the timeline to update onion skin positions
    }

    // During active timeline scrubbing, defer the expensive per-frame work:
    //   - updateActiveFrames: iterates all layers × 7 frames; the pool is
    //     already warm from the previous tick, so deferring is safe.
    //   - updateUndoContext: walks the keyframe tree and does a dynamic_cast
    //     on every frame; only meaningful when the user has stopped scrubbing.
    // Both are scheduled via a zero-delay timer so they fire once after the
    // scrub drag ends (when the event loop is idle).
    if (mTimelineScrubbing) {
        if (!mScrubDeferTimer) {
            mScrubDeferTimer = new QTimer(this);
            mScrubDeferTimer->setSingleShot(true);
            mScrubDeferTimer->setInterval(0);
            connect(mScrubDeferTimer, &QTimer::timeout, this, [this]() {
                mObject->updateActiveFrames(mFrame);
                updateUndoContext();
            });
        }
        mScrubDeferTimer->start(); // restart resets the single-shot so it fires once after drag ends
        emit scrubbed(frame);
        return;
    }

    mObject->updateActiveFrames(frame);
    updateUndoContext();  // Update undo stack based on current keyframe
    emit scrubbed(frame);
}

void Editor::scrubForward()
{
    int nextFrame = mFrame + 1;
    if (!playback()->isPlaying()) {
        playback()->playScrub(nextFrame);
    }
    scrubTo(nextFrame);
}

void Editor::scrubBackward()
{
    if (currentFrame() > 1)
    {
        int previousFrame = mFrame - 1;
        if (!playback()->isPlaying()) {
            playback()->playScrub(previousFrame);
        }
        scrubTo(previousFrame);
    }
}

KeyFrame* Editor::addNewKey()
{
    return addKeyFrame(layers()->currentLayerIndex(), currentFrame());
}

KeyFrame* Editor::addKeyFrame(const int layerNumber, int frameIndex)
{
    Layer* layer = mObject->getLayer(layerNumber);
    Q_ASSERT(layer);

    if (!layer->visible())
    {
        mScribbleArea->showLayerNotVisibleWarning();
        return nullptr;
    }

    // Find next available space for a keyframe (where either no key exists or there is an empty sound key)
    while (layer->keyExists(frameIndex))
    {
        if (layer->type() == Layer::SOUND
            && layer->getKeyFrameAt(frameIndex)->fileName().isEmpty()
            && layer->removeKeyFrame(frameIndex))
        {
            break;
        }
        else
        {
            frameIndex += 1;
        }
    }

    const bool ok = layer->addNewKeyFrameAt(frameIndex);
    Q_ASSERT(ok); // We already ensured that there is no keyframe at frameIndex, so this should always succeed
    scrubTo(frameIndex); // currentFrameChanged() emit inside.
    emit frameModified(frameIndex);
    layers()->notifyAnimationLengthChanged();
    return layer->getKeyFrameAt(frameIndex);
}

void Editor::removeKey()
{
    Layer* layer = layers()->currentLayer();
    Q_ASSERT(layer != nullptr);

    if (!layer->visible())
    {
        mScribbleArea->showLayerNotVisibleWarning();
        return;
    }

    const int frameBeforeRemoval = currentFrame();

    // Get the keyframe that covers the current position
    KeyFrame* keyFrame = layer->getKeyFrameWhichCovers(frameBeforeRemoval);

    // Guard: if no keyframe exists at or covering current position, scrub backward
    if (!keyFrame)
    {
        scrubBackward();
        return;
    }

    // Resolve to the actual keyframe position (handles held/exposure frames)
    const int keyFramePosition = keyFrame->pos();

    // Ensure timeline/selection state is consistent: commit transforms and clear selection
    deselectAll();
    // Select the keyframe about to be removed
    layer->setFrameSelected(keyFramePosition, true);

    // Create undo command BEFORE removing the keyframe (captures it for undo)
    KeyFrameRemoveCommand* removeCommand = new KeyFrameRemoveCommand(
        keyFrame,
        layer->id(),
        tr("Remove Keyframe"),
        this
    );

    // Remove the selected keyframe
    layer->removeKeyFrame(keyFramePosition);

    // Push the undo command to the GLOBAL stack (not per-keyframe)
    if (mUndoRedoManager)
    {
        // Temporarily clear active keyframe context to route to global stack
        mUndoRedoManager->undoStack()->push(removeCommand);
        // Context restored by command (if needed) but no explicit call here
    }

    // Notify UI and dependent systems
    layers()->notifyAnimationLengthChanged();
    emit layers()->currentLayerChanged(layers()->currentLayerIndex()); // trigger timeline repaint.
    emit updateTimeLine(); // Force timeline refresh

    // Keep the playhead on the original frame (e.g., hold frame) for seamless workflow
    scrubTo(frameBeforeRemoval);
}

void Editor::scrubNextKeyFrame()
{
    LayerManager* layerManager = layers();
    Layer* currentLayer = layerManager ? layerManager->currentLayer() : nullptr;
    if (!currentLayer) {
        qWarning() << "[KeyframeNavigation] Cannot move forward: no active layer";
        return;
    }

    const int currentPosition = currentFrame();
    int nextPosition = -1;
    currentLayer->foreachKeyFrame([&](KeyFrame* keyframe) {
        if (keyframe->pos() <= currentPosition) {
            return;
        }
        if (!hasNavigableKeyframeContent(currentLayer, keyframe)) {
            qInfo() << "[KeyframeNavigation] Skipping unnumbered/empty keyframe"
                    << currentLayer->id() << "at frame" << keyframe->pos();
            return;
        }
        if (nextPosition < 0 || keyframe->pos() < nextPosition) {
            nextPosition = keyframe->pos();
        }
    });
    if (nextPosition < 0) {
        qInfo() << "[KeyframeNavigation] No later keyframe on active layer"
                << currentLayer->id() << "at frame" << currentPosition;
        return;
    }

    qInfo() << "[KeyframeNavigation] Next keyframe on active layer"
            << currentLayer->id() << currentPosition << "->" << nextPosition;
    scrubTo(nextPosition);
}

void Editor::scrubPreviousKeyFrame()
{
    LayerManager* layerManager = layers();
    Layer* layer = layerManager ? layerManager->currentLayer() : nullptr;
    if (!layer) {
        qWarning() << "[KeyframeNavigation] Cannot move backward: no active layer";
        return;
    }

    const int currentPosition = currentFrame();
    int prevPosition = -1;
    layer->foreachKeyFrame([&](KeyFrame* keyframe) {
        if (keyframe->pos() >= currentPosition) {
            return;
        }
        if (!hasNavigableKeyframeContent(layer, keyframe)) {
            qInfo() << "[KeyframeNavigation] Skipping unnumbered/empty keyframe"
                    << layer->id() << "at frame" << keyframe->pos();
            return;
        }
        if (prevPosition < 0 || keyframe->pos() > prevPosition) {
            prevPosition = keyframe->pos();
        }
    });
    if (prevPosition < 0) {
        qInfo() << "[KeyframeNavigation] No earlier keyframe on active layer"
                << layer->id() << "at frame" << currentPosition;
        return;
    }

    qInfo() << "[KeyframeNavigation] Previous keyframe on active layer"
            << layer->id() << currentPosition << "->" << prevPosition;
    scrubTo(prevPosition);
}

void Editor::switchVisibilityOfLayer(int layerNumber)
{
    Layer* layer = mObject->getLayer(layerNumber);
    if (layer != nullptr) layer->switchVisibility();
    mScribbleArea->onLayerChanged();

    emit updateTimeLine();
}

void Editor::swapLayers(int i, int j)
{
    bool didSwapLayer = mObject->swapLayers(i, j);
    if (!didSwapLayer) { return; }

    if (j < i)
    {
        layers()->setCurrentLayer(j + 1);
    }
    else
    {
        layers()->setCurrentLayer(j - 1);
    }
    emit updateTimeLine();
    mScribbleArea->onLayerChanged();
}

bool Editor::canSwapLayers(int layerIndexLeft, int layerIndexRight) const
{
    return mObject->canSwapLayers(layerIndexLeft, layerIndexRight);
}

void Editor::prepareSave()
{
    for (auto mgr : mAllManagers)
    {
        mgr->save(mObject.get());
    }
}

void Editor::clearCurrentFrame()
{
    mScribbleArea->clearImage();
}

bool Editor::canCopy() const
{
    Layer* layer = layers()->currentLayer();
    if (!layer) {
        return false;
    }

    KeyFrame* keyframe = layer->getLastKeyFrameAtPosition(mFrame);

    // Do not allow starting a new copy/cut operation while a floating
    // selection is active or waiting to be committed. This avoids nested
    // clipboard operations fighting over the temporary buffer.
    if (select() && (select()->hasFloating() || select()->needsFloatingCommit())) {
        return false;
    }

    switch (layer->type())
    {
    case Layer::SOUND:
    case Layer::CAMERA:
        return canCopyFrames(layer);
    case Layer::BITMAP:
    {
        // For bitmap layers, allow copying either the bitmap image at the
        // active keyframe or selected frames in the timeline. Guard against
        // the case where there is no keyframe at the current frame.
        BitmapImage* bitmapImage = nullptr;
        if (keyframe) {
            auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
            bitmapImage = bitmapLayer->getBitmapImageAtFrame(keyframe->pos());
        }
        return canCopyBitmapImage(bitmapImage) || canCopyFrames(layer);
    }
    case Layer::VECTOR:
    {
        // For vector layers, allow copying the vector image at the active
        // keyframe or selected frames. Handle missing keyframe safely.
        VectorImage* vectorImage = keyframe ? static_cast<VectorImage*>(keyframe) : nullptr;
        return canCopyVectorImage(vectorImage) || canCopyFrames(layer);
    }
    default:
        Q_UNREACHABLE();
    }
}

bool Editor::canPaste() const
{
    Layer* layer = layers()->currentLayer();
    if (layer == nullptr)
    {
        return false;
    }

    // Likewise, do not allow paste while a floating selection is active or
    // waiting to be committed.
    if (select() && (select()->hasFloating() || select()->needsFloatingCommit())) {
        return false;
    }
    auto clipboardMan = clipboards();
    auto layerType = layer->type();

    return (layerType == clipboardMan->framesLayerType() && !clipboardMan->framesIsEmpty()) ||
           (layerType == Layer::BITMAP && clipboardMan->getBitmapClipboard().isLoaded()) ||
           (layerType == Layer::VECTOR && !clipboardMan->getVectorClipboard().isEmpty());
}

bool Editor::canCopyFrames(const Layer* layer) const
{
    Q_ASSERT(layer != nullptr);
    return layer->hasAnySelectedFrames();
}

bool Editor::canCopyBitmapImage(BitmapImage* bitmapImage) const
{
    return bitmapImage != nullptr && bitmapImage->isLoaded() && !bitmapImage->bounds().isEmpty();
}

bool Editor::canCopyVectorImage(const VectorImage* vectorImage) const
{
    return vectorImage != nullptr && !vectorImage->isEmpty();
}

// Legacy backup wrappers: forward to UndoRedoManager where applicable.
void Editor::backup(const QString &undoText)
{
    // Legacy stub — undo/redo now handled by UndoRedoManager.
    // Dirty tracking is automatic via didUpdateUndoStack signal.
}

bool Editor::backup(int layerNumber, int frameNumber, const QString &undoText)
{
    Q_UNUSED(layerNumber)
    Q_UNUSED(frameNumber)
    Q_UNUSED(undoText)
    // Not implemented under new QUndoStack-backed system; return false.
    return false;
}

bool Editor::autoLineFillEnabled() const
{
    return mAutoLineFillEnabled;
}

const QVector<QRgb>& Editor::autoLineColors() const
{
    return mAutoLineColors;
}

void Editor::setAutoLineFillEnabled(bool enabled)
{
    if (mAutoLineFillEnabled == enabled) {
        return;
    }
    mAutoLineFillEnabled = enabled;
    scheduleAutoLineSettingsSave();
}

void Editor::setAutoLineColors(const QVector<QRgb>& colors)
{
    mAutoLineColors = colors;
    scheduleAutoLineSettingsSave();
}

QRgb Editor::ignoreFillColor() const
{
    return mIgnoreFillColor;
}

void Editor::setIgnoreFillColor(QRgb color)
{
    if (mIgnoreFillColor == color) return;
    mIgnoreFillColor = color;
    emit ignoreFillColorChanged(color);
}

bool Editor::ignoreFillEnabled() const
{
    return mIgnoreFillEnabled;
}

void Editor::setIgnoreFillEnabled(bool enabled)
{
    if (mIgnoreFillEnabled == enabled) return;
    mIgnoreFillEnabled = enabled;
    emit ignoreFillEnabledChanged(enabled);
}

void Editor::updateUndoContext()
{
    if (!mUndoRedoManager) return;
    
    Layer* currentLayer = layers()->currentLayer();
    if (!currentLayer) {
        mUndoRedoManager->setActiveCellStack(nullptr);
        return;
    }
    
    if (currentLayer->type() != Layer::BITMAP) {
        mUndoRedoManager->setActiveCellStack(nullptr);
        return;
    }

    if (layers()->getIndex(currentLayer) == -1) {
        mUndoRedoManager->setActiveCellStack(nullptr);
        return;
    }
    
    KeyFrame* keyframe = currentLayer->getLastKeyFrameAtPosition(mFrame);
    if (!keyframe) {
        mUndoRedoManager->setActiveCellStack(nullptr);
        return;
    }
    
    BitmapKeyFrame* bitmapKF = dynamic_cast<BitmapKeyFrame*>(keyframe);
    if (!bitmapKF) {
        mUndoRedoManager->setActiveCellStack(nullptr);
        return;
    }
    
    int cellId = bitmapKF->cellId();
    if (cellId == -1 || !bitmapKF->getCellBank()) {
        mUndoRedoManager->setActiveCellStack(nullptr);
        return;
    }
    
    QUndoStack* cellStack = bitmapKF->getCellBank()->getOrCreateUndoStack(cellId);
    mUndoRedoManager->setActiveCellStack(cellStack);
}

void Editor::onActiveInnerLayerChanged(int cellId, int newIndex)
{
    if (!mUndoRedoManager) return;

    Layer* currentLayer = layers()->currentLayer();
    if (!currentLayer || currentLayer->type() != Layer::BITMAP) return;

    LayerBitmap* layerBitmap = static_cast<LayerBitmap*>(currentLayer);
    CellBank* bank = layerBitmap->cellBank();
    if (!bank) return;

    // Validate that cellId exists in the bank
    CellLayerStack* stack = bank->getCellLayerStack(cellId);
    if (!stack) {
        qWarning() << "Editor::onActiveInnerLayerChanged: cellId" << cellId << "not found in bank";
        return;
    }

    // Validate that newIndex is within bounds
    if (newIndex < 0 || newIndex >= stack->layerCount()) {
        qWarning() << "Editor::onActiveInnerLayerChanged: newIndex" << newIndex
                   << "out of bounds (layerCount=" << stack->layerCount() << ")";
        return;
    }

    // Update the active inner layer index on the cell stack
    bank->setActiveInnerLayerIndex(cellId, newIndex);

    // Route the UndoRedoManager to the newly active inner layer's undo stack
    QUndoStack* newStack = bank->getActiveInnerLayerUndoStack(cellId);
    if (!newStack) {
        qWarning() << "Editor::onActiveInnerLayerChanged: getActiveInnerLayerUndoStack returned null for cellId" << cellId;
    }
    mUndoRedoManager->setActiveCellStack(newStack); // null is acceptable here

    // Refresh the canvas so the newly active layer is reflected
    updateFrame();
}

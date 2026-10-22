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

#include "scribblearea.h"

#include <cmath>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPixmapCache>
#include <QTimer>
#include <QDebug>
#include <QtConcurrent>
#include <QPainter>
#include <QImage>
#include <QShowEvent>
#include <QNativeGestureEvent>

#include "pointerevent.h"
#include "beziercurve.h"
#include "object.h"
#include "editor.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "layervector.h"
#include "layercamera.h"
#include "bitmapimage.h"
#include "vectorimage.h"
#include "blitrect.h"
#include "tile.h"

#include "onionskinpainteroptions.h"

#include "colormanager.h"
#include "toolmanager.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"
#include "overlaymanager.h"
#include "undoredomanager.h"
#include "managers/shifttracemanager.h"
#include "util/pencilsettings.h"
#include "stroketool.h"

#ifdef Q_OS_MACOS
namespace {
constexpr quint32 kMacVirtualKeyZ = 6;

bool isUndoRedoZKey(const QKeyEvent* event)
{
    return event && event->nativeVirtualKey() == kMacVirtualKeyZ &&
           (event->modifiers() & Qt::ControlModifier) &&
           !(event->modifiers() & (Qt::AltModifier | Qt::MetaModifier));
}
}
#else
namespace {
bool isUndoRedoZKey(const QKeyEvent* event)
{
    return event && (event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Z;
}
}
#endif

static bool eventMatchesShortcut(const QKeyEvent* event, const QString& commandId)
{
    // commandId is like CMD_TEMP_PAN_HOLD (macro value string, e.g. "CmdTempPanHold")
    const QString keyPath = QString("shortcuts/%1").arg(commandId);
    const QString seqPortable = pencilSettings().value(keyPath).toString();
    if (seqPortable.isEmpty()) return false;
    const QKeySequence configured(seqPortable);
    // Compare against a single-keystroke QKeyEvent by converting event to QKeySequence
    QKeySequence evSeq(event->modifiers() | event->key());
    // Use NativeText compare to account for localized names
    return evSeq.toString(QKeySequence::PortableText) == configured.toString(QKeySequence::PortableText);
}

void ScribbleArea::drawTaperedCurveNoClear(const QPainterPath& path, QPen& pen, qreal taperStartPixels, qreal taperEndPixels, bool useAA)
{
    // Same as drawTaperedCurve but do not clear the buffer or call beginDrawingStroke
    const qreal totalLength = path.length();
    const qreal fullWidth = pen.widthF();

    if (!(totalLength >= 2.0)) {
        drawPolyline(path, pen, useAA);
        return;
    }

    const qreal startTaperLength = qMax<qreal>(0.0, taperStartPixels);
    const qreal endTaperLength   = qMax<qreal>(0.0, taperEndPixels);

    const QColor strokeColor = pen.color();

    // Adaptive spacing via brush width
    const qreal spacing    = qMin(qMax<qreal>(1.0, 0.25 * fullWidth), qMax<qreal>(1.0, 0.45 * fullWidth));
    const int   maxSamples = 2000;

    // Compute sample count
    int sampleCount = qCeil(totalLength / spacing) + 1;
    sampleCount = qBound(2, sampleCount, maxSamples);

    for (int i = 0; i < sampleCount; ++i) {
        const qreal t = (sampleCount > 1) ? (qreal(i) / qreal(sampleCount - 1)) : 0.0;
        const qreal distance = t * totalLength;
        const QPointF point = path.pointAtPercent(t);

        qreal startRamp = 1.0;
        if (startTaperLength > 0.0) startRamp = qMin<qreal>(1.0, distance / startTaperLength);
        qreal endRamp = 1.0;
        if (endTaperLength > 0.0) endRamp = qMin<qreal>(1.0, (totalLength - distance) / endTaperLength);
        const qreal ramp = qMin(startRamp, endRamp);
        qreal width = 1.0 + (fullWidth - 1.0) * ramp;
        if (width < 1.0) width = 1.0;

        drawPen(point, width, strokeColor, useAA);
    }

    // Update only the affected region
    QRectF updateRegion = path.boundingRect();
    const qreal padding = fullWidth * 1.5;
    updateRegion = updateRegion.adjusted(-padding, -padding, padding, padding);
    updateCanvasRegion(updateRegion);
}

void ScribbleArea::eraseStrokeAlongPath(const QPainterPath& path, qreal width, bool useAA)
{
    // Draw the path using CompositionMode_Clear with a wide round-cap pen to punch a hole
    QPen clearPen(QColor(0,0,0,0), width * 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    mTiledBuffer.drawPath(path, clearPen, Qt::NoBrush, QPainter::CompositionMode_Clear, useAA);
    // Update region
    QRectF updateRegion = path.boundingRect();
    const qreal padding = width * 1.5;
    updateRegion = updateRegion.adjusted(-padding, -padding, padding, padding);
    updateCanvasRegion(updateRegion);
}

ScribbleArea::ScribbleArea(QWidget* parent) : QWidget(parent),
    mCanvasPainter(mCanvas),
    mCameraPainter(mCanvas)
{
    setObjectName("ScribbleArea");

    // WA_StaticContents would tell Qt that only newly-exposed areas need repainting
    // on resize, but the canvas content changes on pan/zoom/tool operations so we
    // must NOT set it — stale regions would persist after view transforms.
    // setAttribute(Qt::WA_StaticContents); // intentionally removed
    
    // Initialize progressive commit tracking
    mProgressiveCommitDistance = 0.0;
    mLastProgressiveCommitPoint = QPointF(0, 0);

    // Canvas update timer for deferred/batched canvas region updates
    mCanvasUpdateTimer = new QTimer(this);
    mCanvasUpdateTimer->setSingleShot(true);
    mCanvasUpdateTimer->setInterval(CANVAS_UPDATE_DELAY_MS);
    connect(mCanvasUpdateTimer, &QTimer::timeout, this, &ScribbleArea::flushCanvasUpdates);

    // Marching-ants animation timer (deferred updates for selection boundary)
    mSelectionAntsTimer = new QTimer(this);
    mSelectionAntsTimer->setInterval(80); // ~12.5 fps; light CPU while still visible
    connect(mSelectionAntsTimer, &QTimer::timeout, this, [this]{
        if (!mEditor) return;
        auto sel = mEditor->select();
        if (!sel || sel->selectionPathRaw().isEmpty()) return;
        // Update selection visuals region in canvas space, including ring/handles padding
        QPainterPath p = sel->selectionPathWithOffsets();
        // Map by selectionTransform to match painter when transforming (scale/rotate)
        if (!sel->selectionTransform().isIdentity()) {
            p = sel->selectionTransform().map(p);
        }
        QRectF baseRect = p.boundingRect();
        // Convert device-space handle padding to canvas units (covers corner + rotation handles)
        const qreal inv = mEditor->view()->getScaleInversed();
        constexpr qreal AA_PAD = 10.0;
        constexpr qreal HANDLE_SIZE = 12.0;
        const qreal pad = (HANDLE_SIZE / 2.0 + AA_PAD) * inv;
        QRectF canvasRect = baseRect.adjusted(-pad, -pad, pad, pad);
        updateCanvasRegion(canvasRect);
    });
}

ScribbleArea::~ScribbleArea()
{
}

void ScribbleArea::setPlaybackPresentationModeEnabled(bool enabled)
{
    if (mPlaybackPresentationModeEnabled == enabled) {
        return;
    }

    // If disabling while active, restore immediately.
    if (!enabled && mPlaybackPresentationModeActive && mEditor && mEditor->view())
    {
        ViewManager* vm = mEditor->view();
        vm->flipHorizontal(mPlaybackPresentationSavedFlipH);
        vm->flipVertical(mPlaybackPresentationSavedFlipV);
        vm->rotate(static_cast<float>(mPlaybackPresentationSavedRotation));
        vm->translate(mPlaybackPresentationSavedTranslation);
        vm->scale(mPlaybackPresentationSavedScaling);
        mPlaybackPresentationModeActive = false;
    }

    mPlaybackPresentationModeEnabled = enabled;
    mCameraPainter.setPlaybackPresentationMode(enabled);

    invalidatePainterCaches();
    update();
}

void ScribbleArea::setCanvasInteractionLocked(bool locked)
{
    if (mCanvasInteractionLocked == locked) {
        return;
    }

    mCanvasInteractionLocked = locked;

    if (mCanvasInteractionLocked && mEditor && mEditor->tools()) {
        mEditor->tools()->endTemporaryTool();
    }

    updateToolCursor();
    update();
}

void ScribbleArea::resetPointerState()
{
    // Called from a tool's onContextMenuResult(), which runs synchronously inside
    // the call stack of mousePressEvent(). Simply clearing mMouseInUse here is not
    // enough because mousePressEvent will re-set it to true after we return.
    // We set mPointerStateResetPending so that mousePressEvent skips the re-assignment.
    mMouseInUse = false;
    mTabletInUse = false;
    mPointerStateResetPending = true;
}

bool ScribbleArea::init()
{
    mPrefs = mEditor->preference();
    mDoubleClickTimer = new QTimer(this);
    mMouseFilterTimer = new QTimer(this);

    connect(mPrefs, &PreferenceManager::optionChanged, this, &ScribbleArea::settingUpdated);
    connect(mEditor->tools(), &ToolManager::toolPropertyChanged, this, &ScribbleArea::onToolPropertyUpdated);
    connect(mEditor->tools(), &ToolManager::toolChanged, this, &ScribbleArea::onToolChanged);
    if (mEditor && mEditor->shiftTrace()) {
        connect(mEditor->shiftTrace(), &ShiftTraceManager::needsRedraw, this, [this]{
            mCanvasPainter.invalidatePreLayersCache();
            invalidateFastNavCache();
            int key = mEditor->layers()->lastFrameAtFrame(mEditor->currentFrame());
            if (key < 0) key = mEditor->currentFrame();
            invalidateCacheForFrame(key);
            // Force repaint even if tools are active
            update();
        });
        connect(mEditor->shiftTrace(), &ShiftTraceManager::hideAllChanged, this, [this](bool){
            // Every cached frame includes its onion skins. A master toggle must
            // invalidate all of them, including frames not currently displayed.
            for (const auto& key : mPixmapCacheKeys) QPixmapCache::remove(key);
            mPixmapCacheKeys.clear();
            mCameraPainter.resetCache();
        });
        connect(mEditor->shiftTrace(), &ShiftTraceManager::activeMoveEntryChanged, 
                this, &ScribbleArea::onShiftTraceMoveStateChanged);
    }

    connect(mDoubleClickTimer, &QTimer::timeout, this, &ScribbleArea::handleDoubleClick);
    connect(mMouseFilterTimer, &QTimer::timeout, this, &ScribbleArea::tabletReleaseEventFired);

    connect(mEditor->select(), &SelectionManager::selectionChanged, this, &ScribbleArea::onSelectionChanged);
    connect(mEditor->select(), &SelectionManager::needDeleteSelection, this, &ScribbleArea::deleteSelection);

    connect(&mTiledBuffer, &TiledBuffer::tileUpdated, this, &ScribbleArea::onTileUpdated);
    connect(&mTiledBuffer, &TiledBuffer::tileCreated, this, &ScribbleArea::onTileCreated);

    // Tile flush timer for batching tileUpdated signals
    mTileFlushTimer = new QTimer(this);
    mTileFlushTimer->setSingleShot(true);
    mTileFlushTimer->setInterval(TILE_FLUSH_DELAY_MS);
    connect(mTileFlushTimer, &QTimer::timeout, this, &ScribbleArea::flushTileUpdates);

    mDoubleClickTimer->setInterval(50);
    mMouseFilterTimer->setInterval(50);

    const int curveSmoothingLevel = mPrefs->getInt(SETTING::CURVE_SMOOTHING);
    mCurveSmoothingLevel = curveSmoothingLevel / 20.0; // default value is 1.0

    mMakeInvisible = false;

    mLayerVisibility = static_cast<LayerVisibility>(mPrefs->getInt(SETTING::LAYER_VISIBILITY));

    mDeltaFactor = mEditor->preference()->isOn(SETTING::INVERT_SCROLL_ZOOM_DIRECTION) ? -1 : 1;

    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);

    setMouseTracking(true); // reacts to mouse move events, even if the button is not pressed
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    setTabletTracking(true); // tablet tracking first added in 5.9
#endif

    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));

    QPixmapCache::setCacheLimit(100 * 1024); // unit is kb, so it's 100MB cache
    mPixmapCacheKeys.clear();

    // Initialize tile batching state from preference
    mTiledBuffer.setBatchingEnabled(mPrefs->isOn(SETTING::PENCIL_TILE_BATCHING_ENABLED));

    return true;
}

void ScribbleArea::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!mEditor || !mEditor->tools()) {
        return;
    }

    BaseTool* tool = mEditor->tools()->currentTool();
    if (auto* strokeTool = dynamic_cast<StrokeTool*>(tool)) {
        strokeTool->updateCanvasCursor(true);
    }
    updateToolCursor();
}

void ScribbleArea::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    mDevicePixelRatio = devicePixelRatioF();
    mCanvas = QPixmap(QSizeF(size() * mDevicePixelRatio).toSize());
    mCanvas.setDevicePixelRatio(mDevicePixelRatio);
    mEditor->view()->setCanvasSize(size());

    invalidateCacheForFrame(mEditor->currentFrame());
    invalidatePainterCaches();
    mCanvasPainter.reset();
    mCameraPainter.reset();

    if (mPlaybackPresentationModeEnabled && mPlaybackPresentationModeActive && mEditor && mEditor->playback() && mEditor->playback()->isPlaying()) {
        updatePresentationViewTransform();
    }
}

void ScribbleArea::settingUpdated(SETTING setting)
{
    switch (setting)
    {
    case SETTING::CURVE_SMOOTHING:
        setCurveSmoothing(mPrefs->getInt(SETTING::CURVE_SMOOTHING));
        break;
    case SETTING::TOOL_CURSOR:
        updateToolCursor();
        break;
    case SETTING::ONION_PREV_FRAMES_NUM:
    case SETTING::ONION_NEXT_FRAMES_NUM:
    case SETTING::ONION_MIN_OPACITY:
    case SETTING::ONION_MAX_OPACITY:
        invalidateAllCache();
        break;
    case SETTING::ANTIALIAS:
    case SETTING::GRID:
    case SETTING::GRID_SIZE_W:
    case SETTING::GRID_SIZE_H:
    case SETTING::OVERLAY_CENTER:
    case SETTING::OVERLAY_THIRDS:
    case SETTING::OVERLAY_GOLDEN:
    case SETTING::OVERLAY_SAFE:
    case SETTING::OVERLAY_PERSPECTIVE1:
    case SETTING::OVERLAY_PERSPECTIVE2:
    case SETTING::OVERLAY_PERSPECTIVE3:
    case SETTING::ACTION_SAFE_ON:
    case SETTING::ACTION_SAFE:
    case SETTING::TITLE_SAFE_ON:
    case SETTING::TITLE_SAFE:
    case SETTING::OVERLAY_SAFE_HELPER_TEXT_ON:
    case SETTING::PREV_ONION:
    case SETTING::NEXT_ONION:
    case SETTING::ONION_BLUE:
    case SETTING::ONION_RED:
    case SETTING::INVISIBLE_LINES:
    case SETTING::OUTLINES:
    case SETTING::ONION_TYPE:
    case SETTING::ONION_WHILE_PLAYBACK:
        invalidateAllCache();
        break;
    case SETTING::ONION_MUTLIPLE_LAYERS:
        invalidateAllCache();
        break;
    case SETTING::LAYER_VISIBILITY_THRESHOLD:
    case SETTING::LAYER_VISIBILITY:
        setLayerVisibility(static_cast<LayerVisibility>(mPrefs->getInt(SETTING::LAYER_VISIBILITY)));
        break;
    case SETTING::INVERT_SCROLL_ZOOM_DIRECTION:
        mDeltaFactor = mEditor->preference()->isOn(SETTING::INVERT_SCROLL_ZOOM_DIRECTION) ? -1 : 1;
        break;
    case SETTING::DIM_INACTIVE_LAYERS:
        // Full cache invalidation needed: the QPixmapCache entries are keyed
        // by frame+dimState, but layer opacity values are not part of the key.
        // Clearing ensures the next repaint recomposites with the correct state.
        invalidateAllCache();
        invalidateFastNavCache();
        break;
    case SETTING::FIELD_W:
    case SETTING::FIELD_H:
        invalidateAllCache();
        invalidateFastNavCache();
        update();
        break;
    case SETTING::CANVAS_COLOR:
        invalidateFastNavCache();
        update();
        break;
    case SETTING::EXTRA_SPACE_STYLE:
    case SETTING::EXTRA_SPACE_SCALE_PERCENT:
    case SETTING::EXTRA_SPACE_MANUAL_W:
    case SETTING::EXTRA_SPACE_MANUAL_H:
        // These settings affect the camera mask/border, so we must redraw immediately.
        // Reset painter caches and invalidate the current frame pixmap cache so it repaints now
        // (otherwise it may not refresh until a stroke occurs).
        invalidatePainterCaches();
        invalidateFastNavCache();
        if (mEditor) {
            invalidateCacheForFrame(mEditor->currentFrame());
        }
        updateFrame();
        break;

    case SETTING::GAP_CHECK_MODE:
    case SETTING::GAP_CHECK_BG_COLOR:
    case SETTING::GAP_CHECK_DRAW_COLOR:
        // Gap Check is a post-processing presentation of the cached canvas.
        // If we don't invalidate the current pixmap cache entry, the view can appear
        // unchanged until a later interaction (e.g. click) triggers a repaint.
        invalidatePainterCaches();
        invalidateFastNavCache();
        if (mEditor) {
            invalidateCacheForFrame(mEditor->currentFrame());
        }
        updateFrame();
        break;
    case SETTING::PENCIL_TILE_BATCHING_ENABLED:
        mTiledBuffer.setBatchingEnabled(mPrefs->isOn(SETTING::PENCIL_TILE_BATCHING_ENABLED));
        break;
    default:
        break;
    }

}

void ScribbleArea::updateToolCursor()
{
    if (mEditor && mEditor->shiftTrace() && mEditor->shiftTrace()->activeMoveEntryId() != -1) {
        setCursor(Qt::ArrowCursor); // keep OS cursor while move mode is active
        return;
    }
    setCursor(currentTool()->cursor());
}

void ScribbleArea::onShiftTraceMoveStateChanged(int)
{
    updateToolCursor();
}

void ScribbleArea::setCurveSmoothing(int newSmoothingLevel)
{
    mCurveSmoothingLevel = newSmoothingLevel / 20.0;
    invalidatePainterCaches();
}

void ScribbleArea::setEffect(SETTING e, bool isOn)
{
    mPrefs->set(e, isOn);
    invalidatePainterCaches();
}

/************************************************************************************/
// update methods

void ScribbleArea::onTileUpdated(TiledBuffer* tiledBuffer, Tile* tile)
{
    Q_UNUSED(tiledBuffer);
    // Sync QImage→QPixmap immediately so paintEvent sees the latest content.
    tile->syncPixmapFromImage();
    updateCanvasRegion(tile->bounds());
    scheduleTileFlush();
}

void ScribbleArea::onTileCreated(TiledBuffer* tiledBuffer, Tile* tile)
{
    Q_UNUSED(tiledBuffer)
    tile->syncPixmapFromImage();
    updateCanvasRegion(tile->bounds());
}

void ScribbleArea::scheduleTileFlush()
{
    if (!mTileFlushTimer) return;
    // Only start the single-shot timer if it is not already active.
    // This ensures that flushes occur at a regular cadence during
    // continuous drawing instead of being indefinitely postponed by
    // frequent dab updates.
    if (!mTileFlushTimer->isActive()) {
        mTileFlushTimer->start();
    }
}

void ScribbleArea::scheduleCanvasUpdate()
{
    // Defensive: require editor and view to be present for mapping-based updates
    if (!mEditor || !mEditor->view()) return;
    if (!mCanvasUpdateTimer) return;
    if (mIsStrokeActive) {
        // Batch updates during active strokes at a regular cadence.
        // Do not restart the timer if it is already active; restarting would
        // postpone updates indefinitely during high-frequency input, causing
        // visible "catch up" / pop-in.
        if (!mCanvasUpdateTimer->isActive()) {
            mCanvasUpdateTimer->start();
        }
    } else {
        // If not in stroke, flush immediately for responsiveness
        flushCanvasUpdates();
    }
}

void ScribbleArea::flushCanvasUpdates()
{
    // Defensive: need valid editor and view to map canvas->screen
    if (!mEditor || !mEditor->view()) return;
    if (mPendingUpdateRegion.isEmpty()) return;
    QRect rect = mEditor->view()->mapCanvasToScreen(mPendingUpdateRegion).toRect();
    rect.adjust(-1, -1, 1, 1); // expand slightly to avoid AA edge clipping
    update(rect);
    mPendingUpdateRegion = QRectF();
}

void ScribbleArea::setStrokeActive(bool active)
{
    mIsStrokeActive = active;
    if (!active) {
        // Ensure final visibility when stroke ends
        flushCanvasUpdates();
    }
}

void ScribbleArea::flushTileUpdates()
{
    {
        auto tiles = mTiledBuffer.tiles();
        for (auto it = tiles.begin(); it != tiles.end(); ++it) {
            Tile* t = it.value();
            if (t) t->syncPixmapFromImage();
        }
    }
    mTiledBuffer.flushDirtyTiles();
    if (!mEditor || !mEditor->view()) return;
    if (!mPendingUpdateRegion.isEmpty()) {
        QRect rect = mEditor->view()->mapCanvasToScreen(mPendingUpdateRegion).toRect();
        rect.adjust(-1, -1, 1, 1);
        update(rect);
        mPendingUpdateRegion = QRectF();
    } else {
        QRect rect = mEditor->view()->mapCanvasToScreen(mTiledBuffer.bounds()).toRect();
        rect.adjust(-1, -1, 1, 1);
        update(rect);
    }
}

void ScribbleArea::updateCanvasRegion(const QRectF& canvasBounds)
{
    if (mPendingUpdateRegion.isEmpty()) {
        mPendingUpdateRegion = canvasBounds;
    } else {
        mPendingUpdateRegion = mPendingUpdateRegion.united(canvasBounds);
    }
    // Defer actual screen update to scheduleCanvasUpdate which is stroke-aware
    scheduleCanvasUpdate();
}

void ScribbleArea::updateFrame()
{
    // Defer full-canvas updates during active drawing to avoid expensive repaints.
    if (mIsStrokeActive && currentTool()->isDrawingTool()) {
        // Defer to batched region updates during strokes
        scheduleCanvasUpdate();
        return;
    }

    update();
}

void ScribbleArea::invalidateCacheForDirtyFrames()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    for (int pos : currentLayer->dirtyFrames()) {

        invalidateCacheForFrame(pos);
        invalidateOnionSkinsCacheAround(pos);
    }
    currentLayer->clearDirtyFrames();
}

void ScribbleArea::invalidateOnionSkinsCacheAround(int frameNumber)
{
    if (frameNumber < 0) { return; }

    bool isOnionAbsolute = mPrefs->getString(SETTING::ONION_TYPE) == "absolute";
    Layer *layer = mEditor->layers()->currentLayer(0);

    // The current layer can be null if updateFrame is triggered when creating a new project
    if (!layer) return;

    if (mPrefs->isOn(SETTING::PREV_ONION))
    {
        int onionFrameNumber = frameNumber;
        if (isOnionAbsolute)
        {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber + 1, true);
        }

        for(int i = 1; i <= mPrefs->getInt(SETTING::ONION_PREV_FRAMES_NUM); i++)
        {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isOnionAbsolute);
            if (onionFrameNumber < 0) break;

            invalidateCacheForFrame(onionFrameNumber);
        }
    }

    if (mPrefs->isOn(SETTING::NEXT_ONION))
    {
        int onionFrameNumber = frameNumber;

        for(int i = 1; i <= mPrefs->getInt(SETTING::ONION_NEXT_FRAMES_NUM); i++)
        {
            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, isOnionAbsolute);
            if (onionFrameNumber < 0) break;

            invalidateCacheForFrame(onionFrameNumber);
        }
    }
}

void ScribbleArea::invalidateAllCache()
{
    if (currentTool()->isDrawingTool() && currentTool()->isActive()) { return; }

    QPixmapCache::clear();
    mPixmapCacheKeys.clear();
    invalidatePainterCaches();
    Layer* curLayer = mEditor->layers()->currentLayer();
    if (curLayer) {
        curLayer->clearDirtyFrames();
    }

    updateFrame();
}

void ScribbleArea::invalidateCacheForFrame(int frameNumber)
{
    // We maintain two cached variants per frame: dim-off (suffix 0) and
    // dim-on (suffix 1). Invalidate both so future repaints rebuild with
    // fresh content.
    const unsigned int baseIndex = static_cast<unsigned int>(frameNumber) * 2u;
    for (unsigned int suffix = 0; suffix < 2; ++suffix) {
        const unsigned int cacheIndex = baseIndex + suffix;
        auto it = mPixmapCacheKeys.find(cacheIndex);
        if (it != mPixmapCacheKeys.end()) {
            QPixmapCache::remove(it.value());
            mPixmapCacheKeys.remove(it.key());
        }
    }
}

void ScribbleArea::invalidatePainterCaches()
{
    mCameraPainter.resetCache();
    mCanvasPainter.resetLayerCache();
    updateFrame();
}

void ScribbleArea::onToolPropertyUpdated(ToolType, ToolPropertyType type)
{
    switch (type)
    {
    case ToolPropertyType::CAMERAPATH:
        onFrameModified(mEditor->currentFrame());
        break;
    default:
        break;
    }
}

void ScribbleArea::onToolChanged(ToolType)
{
    int frame = mEditor->currentFrame();
    prepOverlays(frame);
    prepCameraPainter(frame);
    invalidateCacheForFrame(frame);
    updateFrame();
}


void ScribbleArea::onPlayStateChanged()
{
    int currentFrame = mEditor->currentFrame();

    if (mPlaybackPresentationModeEnabled && mEditor && mEditor->playback() && mEditor->view())
    {
        if (mEditor->playback()->isPlaying())
        {
            if (!mPlaybackPresentationModeActive)
            {
                ViewManager* vm = mEditor->view();
                mPlaybackPresentationSavedTranslation = vm->translation();
                mPlaybackPresentationSavedScaling = vm->scaling();
                mPlaybackPresentationSavedRotation = vm->rotation();
                mPlaybackPresentationSavedFlipH = vm->isFlipHorizontal();
                mPlaybackPresentationSavedFlipV = vm->isFlipVertical();
                mPlaybackPresentationModeActive = true;
            }

            updatePresentationViewTransform();
        }
        else
        {
            if (mPlaybackPresentationModeActive)
            {
                ViewManager* vm = mEditor->view();
                vm->flipHorizontal(mPlaybackPresentationSavedFlipH);
                vm->flipVertical(mPlaybackPresentationSavedFlipV);
                vm->rotate(static_cast<float>(mPlaybackPresentationSavedRotation));
                vm->translate(mPlaybackPresentationSavedTranslation);
                vm->scale(mPlaybackPresentationSavedScaling);
                mPlaybackPresentationModeActive = false;
            }
        }
    }
    if (mPrefs->isOn(SETTING::PREV_ONION) ||
        mPrefs->isOn(SETTING::NEXT_ONION)) {
        invalidatePainterCaches();
    }

    prepOverlays(currentFrame);
    prepCameraPainter(currentFrame);
    invalidateCacheForFrame(currentFrame);
    // Also invalidate pre-layer cache so Shift & Trace hides while playing
    mCanvasPainter.invalidatePreLayersCache();
    updateFrame();
}

void ScribbleArea::onScrubbed(int frameNumber)
{
    Q_UNUSED(frameNumber)

    if (mEditor && mEditor->isTimelineScrubbing()) {
        if (mScrubRepaintThrottle.isValid() && mScrubRepaintThrottle.elapsed() < 16) {
            return;
        }
        mScrubRepaintThrottle.restart();
        mCameraPainter.resetCache();
        updateFrame();
        return;
    }

    invalidatePainterCaches();
    updateFrame();
}

void ScribbleArea::onFramesModified()
{
    invalidateCacheForDirtyFrames();
    if (mPrefs->isOn(SETTING::PREV_ONION) || mPrefs->isOn(SETTING::NEXT_ONION)) {
        invalidatePainterCaches();
    }
    updateFrame();
}

void ScribbleArea::onFrameModified(int frameNumber)
{
    // Resolve to the underlying keyframe index used as the canvas cache key.
    // This makes sure edits/undo on a hold (exposure) correctly invalidate the
    // cached canvas shared by all exposures of the same keyframe.
    int cacheKey = (mEditor && mEditor->layers())
                   ? mEditor->layers()->lastFrameAtFrame(frameNumber)
                   : -1;
    if (cacheKey < 0) {
        cacheKey = frameNumber;
    }

    // Invalidate the CellLayerStack composite for the current bitmap frame so
    // CanvasPainter recomputes it from the updated active inner layer on next render.
    Layer* layer = mEditor->layers() ? mEditor->layers()->currentLayer() : nullptr;
    if (layer && layer->type() == Layer::BITMAP) {
        auto bitmapLayer = static_cast<LayerBitmap*>(layer);
        KeyFrame* kf = bitmapLayer->getLastKeyFrameAtPosition(cacheKey);
        if (kf) {
            BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
            CellLayerStack* stack = bitmapLayer->cellBank()->getCellLayerStack(bkf->cellId());
            if (stack) stack->invalidateComposite();
        }
    }

    if (mPrefs->isOn(SETTING::PREV_ONION) || mPrefs->isOn(SETTING::NEXT_ONION)) {
        invalidateOnionSkinsCacheAround(cacheKey);
        invalidatePainterCaches();
    }
    invalidateCacheForFrame(cacheKey);
    updateFrame();
}

void ScribbleArea::onViewChanged()
{
    // During active navigation, only invalidate painter caches (view transform dependent)
    // but keep QPixmapCache intact to prevent flickering
    if (mIsNavigating) {
        invalidatePainterCaches(); // Updates view transform for rotation/pan
        update(); // Trigger repaint with new transform
        return;
    }
    
    // When not navigating, do full invalidation
    invalidateAllCache();
    invalidateFastNavCache();
}

void ScribbleArea::onLayerChanged()
{
    invalidateAllCache();
}

void ScribbleArea::onSelectionChanged()
{
    int currentFrame = mEditor->currentFrame();
    invalidateCacheForFrame(currentFrame);
    // Ensure layer/camera caches are refreshed so cuts are visible immediately
    invalidatePainterCaches();
    // Start/stop ants timer based on selection state
    if (mSelectionAntsTimer) {
        auto sel = mEditor->select();
        const bool hasSel = sel && sel->somethingSelected() && !sel->selectionPathRaw().isEmpty();
        if (hasSel) {
            if (!mSelectionAntsTimer->isActive()) mSelectionAntsTimer->start();
        } else {
            mSelectionAntsTimer->stop();
        }
    }
    updateFrame();
}

void ScribbleArea::onOnionSkinTypeChanged()
{
    invalidateAllCache();
}

void ScribbleArea::onObjectLoaded()
{
    invalidateAllCache();
    updateFrame();

    if (mPlaybackPresentationModeEnabled && mPlaybackPresentationModeActive && mEditor && mEditor->playback() && mEditor->playback()->isPlaying()) {
        updatePresentationViewTransform();
    }
}

bool ScribbleArea::event(QEvent *event)
{
    bool processed = false;
    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            // macOS reports each pinch update as a magnification delta.
            // Keep the canvas point under the gesture stationary while scaling.
            if (isPointerInUse() || !mEditor || !mEditor->view() ||
                (mEditor->shiftTrace() && mEditor->shiftTrace()->activeMoveEntryId() != -1)) {
                event->accept();
                return true;
            }

            const qreal magnificationDelta = gesture->value();
            const qreal scaleFactor = 1.0 + magnificationDelta;
            if (!std::isfinite(scaleFactor) || scaleFactor <= 0.0) {
                qWarning() << "[ScribbleArea] Ignoring invalid trackpad zoom delta:" << magnificationDelta;
                event->accept();
                return true;
            }

            const qreal newScale = mEditor->view()->scaling() * scaleFactor;
            if (!std::isfinite(newScale) || newScale <= 0.0) {
                qWarning() << "[ScribbleArea] Ignoring invalid trackpad zoom scale:" << newScale;
                event->accept();
                return true;
            }

            startNavigating();
            mEditor->view()->scaleAtOffset(newScale,
                mEditor->view()->mapScreenToCanvas(gesture->localPos()));
            event->accept();
            return true;
        }
    }
    // Intercept shortcuts before QAction to allow tools to override Ctrl+Z during drags
    if (event->type() == QEvent::ShortcutOverride)
    {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        if (isUndoRedoZKey(ke)) {
            // If pointer is in use or lasso has floating selection active, let the tool handle this immediately
            if (isPointerInUse()) {
                if (mEditor && mEditor->tools() && mEditor->tools()->currentTool()) {
                    // Invoke tool handler now so cancel/restore takes effect even if QUndoAction would consume the shortcut
                    mEditor->tools()->currentTool()->keyPressEvent(ke);
                }
                event->accept();
                return true;
            }
        }
    }
    if (event->type() == QEvent::WindowDeactivate)
    {
        // Only end temporary tool when the entire application loses focus.
        // In Qt5, applicationState() returns a single Qt::ApplicationState value,
        // so compare directly instead of using testFlag.
        if (QGuiApplication::applicationState() != Qt::ApplicationActive) {
            editor()->tools()->endTemporaryTool();
        }
        processed = true;
    }
    else if (event->type() == QEvent::FocusOut)
    {
        // When the canvas widget loses focus (for example, when Alt activates the
        // menu bar or focus moves to another dock/widget), make sure any active
        // temporary tool is cancelled so tools like the Alt-based eyedropper
        // cannot remain "stuck" after the key release is consumed elsewhere.
        if (editor() && editor()->tools()) {
            editor()->tools()->endTemporaryTool();
        }
        processed = true;
    } else if (event->type() == QEvent::Enter)
    {
        // Ensure the main window becomes active when hovering back from floating docks
        if (window() && !window()->isActiveWindow()) {
            window()->activateWindow();
            window()->raise();
        }
        if (!hasFocus()) {
            setFocus(Qt::OtherFocusReason);
        }
        // Reset any stale temporary-tool drag state when entering
        if (editor() && editor()->tools()) {
            // Do not end the temp tool here; just ensure cursor is correct
            updateToolCursor();
        }
        processed = currentTool()->enterEvent(static_cast<QEnterEvent*>(event)) || processed;
    } else if (event->type() == QEvent::Leave)
    {
        // Restore the tool cursor on leave to avoid propagating custom cursors
        updateToolCursor();
        processed = currentTool()->leaveEvent(event) || processed;
    }

    return QWidget::event(event) || processed;
}

/************************************************************************/
/* key event handlers                                                   */
/************************************************************************/

void ScribbleArea::keyPressEvent(QKeyEvent *event)
{
    // Don't handle this event on auto repeat
    if (event->isAutoRepeat()) { return; }

    if (mCanvasInteractionLocked)
    {
        const bool isUndo = isUndoRedoZKey(event) && !(event->modifiers() & Qt::ShiftModifier);
        const bool isRedo = (event->modifiers() & Qt::ControlModifier) && (event->key() == Qt::Key_Y);
        const bool isRedoAlt = isUndoRedoZKey(event) && (event->modifiers() & Qt::ShiftModifier);

        if (isUndo || isRedo || isRedoAlt) {
            QWidget::keyPressEvent(event);
            return;
        }

        if (eventMatchesShortcut(event, CMD_TEMP_PAN_HOLD) ||
            eventMatchesShortcut(event, CMD_TEMP_ZOOM_HOLD) ||
            eventMatchesShortcut(event, CMD_TEMP_ROTATE_HOLD) ||
            eventMatchesShortcut(event, CMD_TEMP_TOOL_HAND))
        {
            if (editor()->tools()->beginTemporaryTool(HAND, static_cast<Qt::Key>(event->key()))) {
                return;
            }
        }

        return;
    }

    // Removed Shift & Trace rotate/scale transform triggers (S/R)

    // Escape or Enter exits ghost edit mode
    if (mEditor->shiftTrace()->activeMoveEntryId() != -1) {
        if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            mEditor->shiftTrace()->setActiveMoveEntryId(-1);
            editor()->updateFrame();
            event->accept();
            return;
        }
    }

    // If Shift & Trace edit mode is active, block other tool shortcuts and navigation
    if (mEditor->shiftTrace()->activeMoveEntryId() != -1) {
        return;
    }

    if (isPointerInUse()) {
        bool allowGlobalUndoRedo = false;
        // Allow LassoSelect to handle Ctrl+Z/Ctrl+Shift+Z/Ctrl+Y during active drag to cancel/restore or block redo
        if (currentTool() && currentTool()->type() == ToolType::LASSO_SELECT) {
            const bool ctrl = (event->modifiers() & Qt::ControlModifier);
            const bool shift = (event->modifiers() & Qt::ShiftModifier);
            const bool isUndo = isUndoRedoZKey(event) && !shift;
            const bool isRedo = (isUndoRedoZKey(event) && shift) || (ctrl && event->key() == Qt::Key_Y);
            if (isUndo || isRedo) {
                // If the lasso tool consumes the shortcut (e.g. drag-cancel on
                // Ctrl+Z), stop here. Otherwise, fall through so the global
                // undo/redo action can handle it via QUndoStack.
                if (currentTool()->keyPressEvent(event)) { return; }
                // Do not early-return here; let QWidget/event system dispatch
                // the shortcut to the standard undo/redo actions.
                allowGlobalUndoRedo = true;
            }
        }
        if (!allowGlobalUndoRedo) {
            // For all other shortcuts while dragging (navigation, tool changes,
            // etc.), block them to avoid conflicts with in-progress strokes.
            return;
        }
    }

    if (currentTool()->keyPressEvent(event))
    {
        return; // has been handled by tool
    }

    // --- fixed control key shortcuts ---

    // ---- fixed normal keys ----

    auto selectMan = mEditor->select();
    bool isSomethingSelected = selectMan->somethingSelected();

    // Global deselect: Ctrl+Shift+D
    if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && event->key() == Qt::Key_D) {
        mEditor->deselectAll();
        return;
    }

    if (isSomethingSelected)
    {
        keyEventForSelection(event);
    }
    else
    {
        keyEvent(event);
    }
}

void ScribbleArea::keyEventForSelection(QKeyEvent* event)
{
    // Block selection key events while Shift & Trace edit mode is active
    if (mEditor->shiftTrace()->activeMoveEntryId() != -1) { return; }
    auto selectMan = mEditor->select();
    switch (event->key())
    {
    case Qt::Key_Right:
        selectMan->translate(QPointF(1, 0));
        selectMan->calculateSelectionTransformation();
        emit mEditor->frameModified(mEditor->currentFrame());
        return;
    case Qt::Key_Left:
        selectMan->translate(QPointF(-1, 0));
        selectMan->calculateSelectionTransformation();
        emit mEditor->frameModified(mEditor->currentFrame());
        return;
    case Qt::Key_Up:
        selectMan->translate(QPointF(0, -1));
        selectMan->calculateSelectionTransformation();
        emit mEditor->frameModified(mEditor->currentFrame());
        return;
    case Qt::Key_Down:
        selectMan->translate(QPointF(0, 1));
        selectMan->calculateSelectionTransformation();
        emit mEditor->frameModified(mEditor->currentFrame());
        return;
    case Qt::Key_Return:
        applyTransformedSelection();
        mEditor->deselectAll();
        return;
    case Qt::Key_Escape:
        cancelTransformedSelection();
        mEditor->deselectAll();
        return;
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
        deleteSelection();
        mEditor->deselectAll();
        return;
    case Qt::Key_Space:
        // fallthrough to generic handling below; keep switch structured
        break;
    default:
        break;
    }
    // Configurable temporary holds: Pan/Zoom/Rotate
    if (mEditor->shiftTrace()->activeMoveEntryId() == -1) {
        const bool isLassoSelect = (mEditor->tools()->currentTool() &&
                                   mEditor->tools()->currentTool()->type() == ToolType::LASSO_SELECT);

        if (eventMatchesShortcut(event, CMD_TEMP_PAN_HOLD)) {
            if (editor()->tools()->beginTemporaryTool(HAND, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Pan hold begin"; return; }
        }
        if (eventMatchesShortcut(event, CMD_TEMP_ZOOM_HOLD)) {
            if (editor()->tools()->beginTemporaryTool(HAND, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Zoom hold begin"; return; }
        }
        if (eventMatchesShortcut(event, CMD_TEMP_ROTATE_HOLD)) {
            if (editor()->tools()->beginTemporaryTool(HAND, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Rotate hold begin"; return; }
        }
        // While Lasso Select is active, block non-navigation temporary tools so modifier
        // shortcuts (Ctrl/Shift combos) can't switch tools mid-lasso.
        if (!isLassoSelect) {
            // Temporary gap-fill holds: begin corresponding gap-fill tools (default unassigned)
            if (eventMatchesShortcut(event, CMD_TEMP_RECT_GAP_FILL)) {
                // Use the same key-tracking logic as other temporary holds; store the activating key
                if (editor()->tools()->beginTemporaryTool(RECT_GAP_FILL, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp RectGapFill hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_LASSO_GAP_FILL)) {
                if (editor()->tools()->beginTemporaryTool(LASSO_GAP_FILL, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp LassoGapFill hold begin"; return; }
            }
            // Generic per-tool temporary holds (user-assignable; defaults empty)
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_PEN)) {
                if (editor()->tools()->beginTemporaryTool(PEN, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Pen hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_ERASER)) {
                if (editor()->tools()->beginTemporaryTool(ERASER, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Eraser hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_SELECT)) {
                if (editor()->tools()->beginTemporaryTool(SELECT, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Select hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_MOVE)) {
                if (editor()->tools()->beginTemporaryTool(MOVE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Move hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_POLYLINE)) {
                if (editor()->tools()->beginTemporaryTool(POLYLINE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Polyline hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_CURVE)) {
                if (editor()->tools()->beginTemporaryTool(CURVE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Curve hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_BSPLINE)) {
                if (editor()->tools()->beginTemporaryTool(BSPLINE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp B-Spline hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_BUCKET)) {
                if (editor()->tools()->beginTemporaryTool(BUCKET, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Bucket hold begin"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_EYEDROPPER)) {
                // Canvas-local Alt-based picker: continuous color selection while key is held.
                // Do not use sample-only here; directly update front color via continuousPick.
                if (event->key() == Qt::Key_Alt) {
                    editor()->tools()->setEyedropperSampleOnly(false);
                    editor()->tools()->setEyedropperContinuousPick(true);
                }
                if (editor()->tools()->beginTemporaryTool(EYEDROPPER, static_cast<Qt::Key>(event->key()))) {
                    qDebug() << "[ScribbleArea] Temp Eyedropper hold begin";
                    // Consume the activating key so Alt does not bubble up to the menu bar.
                    event->accept();
                    return;
                }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_DPLATE)) {
                if (editor()->tools()->beginTemporaryTool(DPLATE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Dplate hold begin"; return; }
            }
        }
    }
}

void ScribbleArea::keyEvent(QKeyEvent* event)
{
    // Block general key events while Shift & Trace edit mode is active
    if (mEditor->shiftTrace()->activeMoveEntryId() != -1) { return; }
    switch (event->key())
    {
    case Qt::Key_Right:
        mEditor->scrubForward();
        qInfo() << "[FrameNavigation] Right arrow, frame:" << mEditor->currentFrame();
        break;
    case Qt::Key_Left:
        mEditor->scrubBackward();
        qInfo() << "[FrameNavigation] Left arrow, frame:" << mEditor->currentFrame();
        break;
    case Qt::Key_Up:
        mEditor->layers()->gotoNextLayer();
        break;
    case Qt::Key_Down:
        mEditor->layers()->gotoPreviouslayer();
        break;
    case Qt::Key_Space:
        // handled after switch via settings-driven temp holds
        break;
    default:
        break;
    }
    event->ignore();

    // Global (non-selection) configurable holds
    if (mEditor->shiftTrace()->activeMoveEntryId() == -1) {
        const bool isLassoSelect = (mEditor->tools()->currentTool() &&
                                   mEditor->tools()->currentTool()->type() == ToolType::LASSO_SELECT);

        if (eventMatchesShortcut(event, CMD_TEMP_PAN_HOLD)) {
            if (editor()->tools()->beginTemporaryTool(HAND, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Pan hold begin (global)"; return; }
        }
        if (eventMatchesShortcut(event, CMD_TEMP_ZOOM_HOLD)) {
            if (editor()->tools()->beginTemporaryTool(HAND, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Zoom hold begin (global)"; return; }
        }
        if (eventMatchesShortcut(event, CMD_TEMP_ROTATE_HOLD)) {
            if (editor()->tools()->beginTemporaryTool(HAND, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Pan hold begin (global)"; return; }
        }
        // While Lasso Select is active, block non-navigation temporary tools so modifier
        // shortcuts (Ctrl/Shift combos) can't switch tools mid-lasso.
        if (!isLassoSelect) {
            // Global gap-fill holds
            if (eventMatchesShortcut(event, CMD_TEMP_RECT_GAP_FILL)) {
                if (editor()->tools()->beginTemporaryTool(RECT_GAP_FILL, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp RectGapFill hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_LASSO_GAP_FILL)) {
                if (editor()->tools()->beginTemporaryTool(LASSO_GAP_FILL, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp LassoGapFill hold begin (global)"; return; }
            }
            // Generic per-tool temporary holds (global)
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_PEN)) {
                if (editor()->tools()->beginTemporaryTool(PEN, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Pen hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_ERASER)) {
                if (editor()->tools()->beginTemporaryTool(ERASER, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Eraser hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_SELECT)) {
                if (editor()->tools()->beginTemporaryTool(SELECT, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Select hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_MOVE)) {
                if (editor()->tools()->beginTemporaryTool(MOVE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Move hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_POLYLINE)) {
                if (editor()->tools()->beginTemporaryTool(POLYLINE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Polyline hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_CURVE)) {
                if (editor()->tools()->beginTemporaryTool(CURVE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Curve hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_BSPLINE)) {
                if (editor()->tools()->beginTemporaryTool(BSPLINE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp B-Spline hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_BUCKET)) {
                if (editor()->tools()->beginTemporaryTool(BUCKET, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Bucket hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_EYEDROPPER)) {
                // For global temp eyedropper holds, enable continuous pick if Alt is used
                // as the modifier, otherwise preserve previous sample-only behavior.
                if (event->key() == Qt::Key_Alt) {
                    editor()->tools()->setEyedropperSampleOnly(false);
                    editor()->tools()->setEyedropperContinuousPick(true);
                } else {
                    editor()->tools()->setEyedropperSampleOnly(true);
                    editor()->tools()->setEyedropperContinuousPick(false);
                }
                if (editor()->tools()->beginTemporaryTool(EYEDROPPER, static_cast<Qt::Key>(event->key()))) {
                    qDebug() << "[ScribbleArea] Temp Eyedropper hold begin (global)";
                    event->accept();
                    return;
                }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_DPLATE)) {
                if (editor()->tools()->beginTemporaryTool(DPLATE, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Dplate hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_HAND)) {
                if (editor()->tools()->beginTemporaryTool(HAND, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Hand hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_CAMERA)) {
                if (editor()->tools()->beginTemporaryTool(CAMERA, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp Camera hold begin (global)"; return; }
            }
            if (eventMatchesShortcut(event, CMD_TEMP_TOOL_LASSO_SELECT)) {
                if (editor()->tools()->beginTemporaryTool(LASSO_SELECT, static_cast<Qt::Key>(event->key()))) { qDebug() << "[ScribbleArea] Temp LassoSelect hold begin (global)"; return; }
            }
        }
    }
}

void ScribbleArea::keyReleaseEvent(QKeyEvent *event)
{
    // Don't handle this event on auto repeat
    //
    if (event->isAutoRepeat()) {
        return;
    }

    if (mCanvasInteractionLocked)
    {
        if (editor()->tools()->shouldEndTemporaryTool(static_cast<Qt::Key>(event->key()))) {
            editor()->tools()->endTemporaryTool();
        }
        return;
    }
    // End Shift & Trace transform drag on S/R release
    if ((event->key() == Qt::Key_S || event->key() == Qt::Key_R) && mEditor->shiftTrace()->activeMoveEntryId() != -1) {
        mEditor->shiftTrace()->endDrag();
        update();
    }

    // If pointer is down (mouse or tablet), defer clearing the temporary tool
    if (isPointerInUse()) {
        // Only defer if this key is actually the one that would end the temp tool
        if (editor()->tools()->shouldEndTemporaryTool(static_cast<Qt::Key>(event->key()))) {
            mIsTempToolClearDeferred = true;
            mDeferredTempToolKey = static_cast<Qt::Key>(event->key());
            return;
        }
        // If it's some other key, let it fall through or be handled normally
    }

    // Only end temporary tool if this key was the one that activated it
    if (editor()->tools()->shouldEndTemporaryTool(static_cast<Qt::Key>(event->key()))) {
        if (editor()->tools()->endTemporaryTool()) { qDebug() << "[ScribbleArea] key release temp end"; }
    }

    if (currentTool()->keyReleaseEvent(event))
    {
        // has been handled by tool
        return;
    }
}

/************************************************************************************/
// mouse and tablet event handlers
void ScribbleArea::wheelEvent(QWheelEvent* event)
{
    // Don't change view if the tool is in use
    if (isPointerInUse()) return;
    // Disable zoom while Shift & Trace edit mode is active
    if (mEditor->shiftTrace()->activeMoveEntryId() != -1) { event->accept(); return; }

    static const bool isX11 = QGuiApplication::platformName() == "xcb";
    const QPoint pixels = event->pixelDelta();
    const QPoint angle = event->angleDelta();
    if (!isX11 && !pixels.isNull()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QPointF screenPosition = event->position();
#else
        const QPointF screenPosition = event->posF();
#endif
        const QTransform viewInverse = mEditor->view()->getViewInverse();
        const QPointF screenDelta(pixels.x(), pixels.y());
        const QPointF viewDelta = viewInverse.map(screenPosition + screenDelta) -
                                  viewInverse.map(screenPosition);
        if (!std::isfinite(viewDelta.x()) || !std::isfinite(viewDelta.y())) {
            qWarning() << "[ScribbleArea] Ignoring invalid trackpad pan delta:" << pixels;
            event->accept();
            return;
        }

        // Pixel deltas follow the fingers; translate the view in the same direction.
        startNavigating();
        mEditor->view()->translate(mEditor->view()->translation() + viewDelta);
        event->accept();
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPointF offset = mEditor->view()->mapScreenToCanvas(event->position());
#else
    const QPointF offset = mEditor->view()->mapScreenToCanvas(event->posF());
#endif

    const qreal currentScale = mEditor->view()->scaling();

    // From the pixelDelta documentation: On X11 this value is driver-specific and unreliable, use angleDelta() instead
    int delta = 0;
    if (!isX11 && !pixels.isNull())
    {
        delta = pixels.y();
    }
    else if (!angle.isNull()) // Wheel based delta
    {
        delta = angle.y();
    }

    if (delta != 0) {
        const qreal newScale = currentScale * std::pow(100, (delta * mDeltaFactor) / (12.0 * 120));
        
        // Trigger navigation mode for smooth zooming
        startNavigating();
        
        mEditor->view()->scaleAtOffset(newScale, offset);
    }
    event->accept();
}

void ScribbleArea::updatePresentationViewTransform()
{
    if (!mEditor || !mEditor->view() || !mEditor->layers() || !mEditor->playback()) {
        return;
    }
    if (!mPlaybackPresentationModeEnabled || !mEditor->playback()->isPlaying()) {
        return;
    }

    LayerCamera* cameraLayerBelow = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
    if (!cameraLayerBelow) {
        return;
    }

    const int frame = mEditor->currentFrame();
    const QTransform camTransform = cameraLayerBelow->getViewAtFrame(frame);
    const QRect cameraRect = cameraLayerBelow->getViewRect();
    const QPolygon camPoly = camTransform.inverted().map(QPolygon(cameraRect));
    const QRectF camBounds = camPoly.boundingRect();
    if (camBounds.isEmpty()) {
        return;
    }

    ViewManager* vm = mEditor->view();

    // Reset to a stable baseline for deterministic fit.
    vm->flipHorizontal(false);
    vm->flipVertical(false);
    vm->resetRotation();
    vm->centerView();
    vm->scale(1.0);

    const QRectF initialScreenBounds = vm->mapCanvasToScreen(camBounds);
    if (initialScreenBounds.isEmpty()) {
        return;
    }

    const qreal vw = qMax<qreal>(1.0, static_cast<qreal>(width()));
    const qreal vh = qMax<qreal>(1.0, static_cast<qreal>(height()));
    const qreal sx = vw / qMax<qreal>(1.0, initialScreenBounds.width());
    const qreal sy = vh / qMax<qreal>(1.0, initialScreenBounds.height());
    const qreal factor = qMax<qreal>(0.01, qMin(sx, sy));

    vm->scaleAtOffset(factor, camBounds.center());

    const QRectF afterScaleBounds = vm->mapCanvasToScreen(camBounds);
    const QPointF desiredCenter(vw * 0.5, vh * 0.5);
    const QPointF currentCenter = afterScaleBounds.center();
    const QPointF deltaCanvas = vm->mapScreenToCanvas(desiredCenter) - vm->mapScreenToCanvas(currentCenter);
    vm->translate(vm->translation() + deltaCanvas);

    vm->forceUpdateViewTransform();
}

void ScribbleArea::tabletEvent(QTabletEvent *e)
{
    if (mCanvasInteractionLocked) {
        e->accept();
        return;
    }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->posF()));

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (event.pointerType() == QPointingDevice::PointerType::Eraser)
#else
    if (event.pointerType() == QTabletEvent::Eraser)
#endif
    {
        editor()->tools()->tabletSwitchToEraser();
    }
    else
    {
        editor()->tools()->tabletRestorePrevTool();
    }

    if (event.eventType() == PointerEvent::Press)
    {
        event.accept();
        if (mIsFirstClick)
        {
            mIsFirstClick = false;
            mDoubleClickTimer->start();
            pointerPressEvent(&event);
        }
        else
        {
            qreal distance = QLineF(e->posF(), mTabletPressPos).length();
            mTabletPressPos = e->posF();

            if (mDoubleClickMillis <= DOUBLE_CLICK_THRESHOLD && distance < 5.0) {
                currentTool()->pointerDoubleClickEvent(&event);
            }
            else
            {
                // in case we handled the event as double click but really should have handled it as single click.
                pointerPressEvent(&event);
            }
        }
        if (mPointerStateResetPending) {
            mPointerStateResetPending = false;
            mTabletInUse = false;
        } else {
            mTabletInUse = event.isAccepted();
        }
    }
    else if (event.eventType() == PointerEvent::Move)
    {
        if (!(event.buttons() & (Qt::LeftButton | Qt::RightButton)) || mTabletInUse)
        {
            // During navigation (hand tool), cap tablet move events to ~70fps.
            // Tablets fire 200-500 events/sec which is far more than the screen
            // can render — skip events that arrive faster than TABLET_NAV_THROTTLE_MS.
            if (mIsNavigating) {
                if (mTabletNavThrottle.isValid() &&
                    mTabletNavThrottle.elapsed() < TABLET_NAV_THROTTLE_MS) {
                    e->accept();
                    return;
                }
                mTabletNavThrottle.restart();
            }
            pointerMoveEvent(&event);
        }
    }
    else if (event.eventType() == PointerEvent::Release)
    {
        mTabletReleaseMillisAgo = 0;
        mMouseFilterTimer->start();
        if (mTabletInUse)
        {
            pointerReleaseEvent(&event);
            mTabletInUse = false;
            // Now that the tablet pointer is no longer in use, clear deferred temp tool if any
            if (mIsTempToolClearDeferred && !isPointerInUse()) {
                if (editor()->tools()->shouldEndTemporaryTool(mDeferredTempToolKey)) {
                    editor()->tools()->endTemporaryTool();
                }
                mIsTempToolClearDeferred = false;
                mDeferredTempToolKey = Qt::Key_unknown;
            }
        }
    }

#if defined Q_OS_LINUX
    // Generate mouse events on linux to work around bug where the
    // widget will not receive mouseEnter/mouseLeave
    // events and the cursor will not update correctly.
    // See https://codereview.qt-project.org/c/qt/qtbase/+/255384
    // Scribblearea should not do anything with the mouse event when mTabletInUse is true.
    event.ignore();
#else
    // Always accept so that mouse events are not generated (theoretically)
    // Unfortunately Windows sometimes generates the events anyway
    // As long as mTabletInUse is true, mouse events *should* be ignored even when
    // the are generated
    event.accept();
#endif
}

void ScribbleArea::pointerPressEvent(PointerEvent* event)
{
    if (mCanvasInteractionLocked) {
        if (currentTool() && currentTool()->type() == ToolType::HAND) {
            currentTool()->pointerPressEvent(event);
            return;
        }
        if ((event->buttons() & Qt::MiddleButton) && editor()->tools()->beginTemporaryTool(HAND, Qt::Key_unknown))
        {
            currentTool()->pointerPressEvent(event);
            return;
        }
        event->accept();
        return;
    }

    // Shift & Trace manual move mode: intercept press to begin dragging the active entry
    if (editor()->shiftTrace()->activeMoveEntryId() != -1)
    {
        if (event->button() == Qt::LeftButton) {
            // Hit-test gizmo to pick drag mode
            auto stm = editor()->shiftTrace();
            int id = stm->activeMoveEntryId();
            ShiftTraceManager::DragMode mode = ShiftTraceManager::DragMode::Translate;
            // Find entry
            ShiftTraceManager::Entry entry;
            bool found = false;
            for (const auto& e : stm->entries()) { if (e.id == id) { entry = e; found = true; break; } }
            if (found) {
                const QPointF clickScreen = mEditor->view()->mapCanvasToScreen(event->canvasPos());

                // Build the same canvas-space transform the painter uses
                const qreal s    = qMax<qreal>(0.01, qIsFinite(entry.scale) ? entry.scale : 1.0);
                const qreal rDeg = qIsFinite(entry.rotationDeg) ? entry.rotationDeg : 0.0;
                QTransform canvasT;
                canvasT.translate(entry.pivot.x(), entry.pivot.y());
                if (!qFuzzyIsNull(rDeg))    canvasT.rotate(rDeg);
                if (!qFuzzyCompare(s, 1.0)) canvasT.scale(s, s);
                canvasT.translate(-entry.pivot.x(), -entry.pivot.y());
                canvasT.translate(entry.offset.x(), entry.offset.y());
                const QTransform fullT = canvasT * mEditor->view()->getView();

                // Compute screen corners (TL=0, TR=1, BR=2, BL=3)
                QVector<QPointF> sc;
                QRectF canvasBounds;
                Layer* layer = mEditor->layers()->getLayer(entry.layerIndex);
                if (layer && layer->type() == Layer::BITMAP) {
                    LayerBitmap* bl = static_cast<LayerBitmap*>(layer);
                    canvasBounds = bl->getFrameBounds(entry.frameNumber);
                } else if (layer && layer->type() == Layer::VECTOR) {
                    LayerVector* vl = static_cast<LayerVector*>(layer);
                    VectorImage* vimg = vl->getVectorImageAtFrame(entry.frameNumber);
                    if (vimg) {
                        QRectF r = vimg->getBoundsOfTransformedCurves();
                        if (r.isEmpty()) r = QRectF(QPointF(0,0), vimg->getSize());
                        canvasBounds = r;
                    }
                }
                if (!canvasBounds.isEmpty()) {
                    sc.resize(4);
                    sc[0] = fullT.map(canvasBounds.topLeft());
                    sc[1] = fullT.map(canvasBounds.topRight());
                    sc[2] = fullT.map(canvasBounds.bottomRight());
                    sc[3] = fullT.map(canvasBounds.bottomLeft());
                }

                bool onScale  = false;
                bool onRotate = false;

                if (sc.size() == 4) {
                    const qreal handleR = 10.0;
                    // Move handle: center of bbox
                    const QPointF moveCenter = (sc[0] + sc[1] + sc[2] + sc[3]) * 0.25;
                    const QRectF moveRect(moveCenter.x() - handleR, moveCenter.y() - handleR, handleR * 2, handleR * 2);
                    // Rotation handle: midpoint of right edge
                    const QPointF rotCenter = (sc[1] + sc[2]) * 0.5;
                    const QRectF rotRect(rotCenter.x() - handleR, rotCenter.y() - handleR, handleR * 2, handleR * 2);

                    // Check rotation first (it sits on the bounding box edge, check before scale)
                    if (rotRect.contains(clickScreen)) {
                        onRotate = true;
                    } else {
                        for (const QPointF& cs : sc) {
                            if (QLineF(cs, clickScreen).length() <= handleR) { onScale = true; break; }
                        }
                    }
                }

                if (onScale)       mode = ShiftTraceManager::DragMode::Scale;
                else if (onRotate) mode = ShiftTraceManager::DragMode::Rotate;
                else               mode = ShiftTraceManager::DragMode::Translate; // move or anywhere else
            }
            editor()->shiftTrace()->beginTransformDrag(mode, event->canvasPos());
            mCanvasPainter.invalidatePreLayersCache();
            int key = mEditor->layers()->lastFrameAtFrame(mEditor->currentFrame());
            if (key < 0) key = mEditor->currentFrame();
            invalidateCacheForFrame(key);
        }
        // Block all buttons from reaching tools while in edit mode (also disables pan via MMB/RMB)
        event->accept();
        return;
    }
    bool isCameraLayer = mEditor->layers()->currentLayer()->type() == Layer::CAMERA;
    if ((currentTool()->type() != HAND || isCameraLayer) && (event->button() != Qt::RightButton) && (event->button() != Qt::MiddleButton || isCameraLayer))
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (!layer->visible())
        {
            event->ignore();
            // This needs to be async so that mTabletInUse is set to false before
            // further events are created (modal dialogs do not currently block tablet events)
            QTimer::singleShot(0, this, &ScribbleArea::showLayerNotVisibleWarning);
            return;
        }

        // Block drawing on a locked inner paper — emit signal so the app can show the popup
        if (layer->type() == Layer::BITMAP) {
            auto* bl = static_cast<LayerBitmap*>(layer);
            const int cf = mEditor->currentFrame();
            KeyFrame* kf = bl->keyExists(cf) ? bl->getKeyFrameAt(cf)
                                             : bl->getLastKeyFrameAtPosition(cf);
            if (auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf)) {
                CellLayerStack* st = bl->cellBank() ? bl->cellBank()->getCellLayerStack(bkf->cellId()) : nullptr;
                if (st && st->activeLayer() && st->activeLayer()->locked) {
                    event->ignore();
                    QTimer::singleShot(0, this, [this](){ emit lockedLayerAttempted(); });
                    return;
                }
            }
        }
    }

    // While using BSPLINE, disable temporary hand tool switching to avoid navigation while editing
    if (currentTool()->type() != BSPLINE &&
        (event->buttons() & (Qt::MiddleButton)) &&
        editor()->tools()->beginTemporaryTool(HAND, Qt::Key_unknown))  // Mouse button activation
    {
        qDebug() << "[ScribbleArea] Mouse temp Hand begin (MMB/RMB)";
        // If Shift & Trace edit mode is active, block temp hand tool
        if (mEditor->shiftTrace()->activeMoveEntryId() != -1) { event->accept(); return; }
        currentTool()->pointerPressEvent(event);
    } else {
        // Forward other button presses to the active tool (e.g., BSpline uses RMB to delete last point)
        if (mEditor->shiftTrace()->activeMoveEntryId() != -1) { event->accept(); return; }
        currentTool()->pointerPressEvent(event);
    }
}

void ScribbleArea::pointerMoveEvent(PointerEvent* event)
{
    if (mCanvasInteractionLocked) {
        if (currentTool() && currentTool()->type() == ToolType::HAND) {
            currentTool()->pointerMoveEvent(event);
            return;
        }
        event->accept();
        return;
    }

    // Shift & Trace move-in-progress
    if (editor()->shiftTrace()->activeMoveEntryId() != -1 && (event->buttons() & Qt::LeftButton))
    {
        editor()->shiftTrace()->updateDrag(event->canvasPos());
        mCanvasPainter.invalidatePreLayersCache();
        int key = mEditor->layers()->lastFrameAtFrame(mEditor->currentFrame());
        if (key < 0) key = mEditor->currentFrame();
        invalidateCacheForFrame(key);
        updateFrame();
        return;
    }
    // Block tools while in edit mode
    if (mEditor->shiftTrace()->activeMoveEntryId() != -1) { event->accept(); return; }
    currentTool()->pointerMoveEvent(event);
}

void ScribbleArea::pointerReleaseEvent(PointerEvent* event)
{
    if (mCanvasInteractionLocked) {
        if (currentTool() && currentTool()->type() == ToolType::HAND) {
            currentTool()->pointerReleaseEvent(event);
            return;
        }
        event->accept();
        return;
    }

    if (editor()->shiftTrace()->activeMoveEntryId() != -1)
    {
        if (event->button() == Qt::LeftButton) {
            editor()->shiftTrace()->endDrag();
            mCanvasPainter.invalidatePreLayersCache();
            int key = mEditor->layers()->lastFrameAtFrame(mEditor->currentFrame());
            if (key < 0) key = mEditor->currentFrame();
            invalidateCacheForFrame(key);
            updateFrame();
        }
        // Do not forward to tools while in edit mode
        event->accept();
        return;
    }
    else
    {
        currentTool()->pointerReleaseEvent(event);
    }

    // End temporary tool if it was activated by mouse buttons (not by a key)
    if (editor()->tools()->shouldEndTemporaryTool(Qt::Key_unknown)) {
        if (editor()->tools()->endTemporaryTool()) { qDebug() << "[ScribbleArea] mouse release temp end"; }
    }

    // If we deferred clearing the temp tool due to key release during a drag, clear it now
    if (mIsTempToolClearDeferred && !isPointerInUse()) {
        if (editor()->tools()->shouldEndTemporaryTool(mDeferredTempToolKey)) {
            editor()->tools()->endTemporaryTool();
        }
        mIsTempToolClearDeferred = false;
        mDeferredTempToolKey = Qt::Key_unknown;
    }
}

void ScribbleArea::handleDoubleClick()
{
    mDoubleClickMillis += 100;

    if (mDoubleClickMillis >= DOUBLE_CLICK_THRESHOLD)
    {
        mDoubleClickMillis = 0;
        mIsFirstClick = true;
        mDoubleClickTimer->stop();
    }
}

void ScribbleArea::tabletReleaseEventFired()
{
    // Under certain circumstances, a mouse press event will fire after a tablet release event.
    // This causes unexpected behaviors for some tools, e.g., the bucket tool.
    // The problem only seems to occur on Windows and only when tapping.
    // Prior to this fix, the event queue would look like this:
    // e.g.: TabletPress -> TabletRelease -> MousePress
    // The following will filter mouse events created after a tablet release event.
    mTabletReleaseMillisAgo += 50;

    if (mTabletReleaseMillisAgo >= MOUSE_FILTER_THRESHOLD) {
        mTabletReleaseMillisAgo = 0;
        mMouseFilterTimer->stop();
    }
}

void ScribbleArea::mousePressEvent(QMouseEvent* e)
{
    if (mTabletInUse || (mMouseFilterTimer->isActive() && mTabletReleaseMillisAgo < MOUSE_FILTER_THRESHOLD))
    {
        e->ignore();
        return;
    }

    // Intercept click on Shift & Trace pivot reset glyph and reset panel buttons if active
    if (mEditor->shiftTrace()->activeMoveEntryId() != -1) {
        const auto stm = mEditor->shiftTrace();
        int id = stm->activeMoveEntryId();
        // Compute reset rect in screen space
        for (const auto& eEntry : stm->entries()) {
            if (eEntry.id == id) {
                QPointF pivotScreen = mEditor->view()->mapCanvasToScreen(eEntry.pivot);
                QPointF resetPos = pivotScreen + QPointF(0, -16);
                QRectF resetRect(resetPos.x() - 8, resetPos.y() - 8, 16, 16);
                if (resetRect.contains(e->localPos())) {
                    // Reset shift frame position (offset) instead of resetting pivot
                    stm->setEntryOffset(id, QPointF(0,0));
                    e->accept();
                    return;
                }
                break;
            }
        }
    }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->localPos()));
    pointerPressEvent(&event);
    if (mPointerStateResetPending) {
        // A context menu was shown during pointerPressEvent. The Qt::Popup window
        // swallowed the mouse release, so we must not re-arm mMouseInUse here.
        mPointerStateResetPending = false;
    } else {
        mMouseInUse = event.isAccepted();
    }
}

void ScribbleArea::mouseMoveEvent(QMouseEvent* e)
{
    if (mTabletInUse || (mMouseFilterTimer->isActive() && mTabletReleaseMillisAgo < MOUSE_FILTER_THRESHOLD)) { e->ignore(); return; }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->localPos()));
    pointerMoveEvent(&event);
}

void ScribbleArea::mouseReleaseEvent(QMouseEvent* e)
{
    if (mTabletInUse || (mMouseFilterTimer->isActive() && mTabletReleaseMillisAgo < MOUSE_FILTER_THRESHOLD)) { e->ignore(); return; }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->localPos()));
    pointerReleaseEvent(&event);
    mMouseInUse = (e->buttons() & Qt::RightButton) || (e->buttons() & Qt::LeftButton);

    // If pointer is no longer in use, clear any deferred temp tool
    if (!isPointerInUse() && mIsTempToolClearDeferred) {
        if (editor()->tools()->shouldEndTemporaryTool(mDeferredTempToolKey)) {
            editor()->tools()->endTemporaryTool();
        }
        mIsTempToolClearDeferred = false;
        mDeferredTempToolKey = Qt::Key_unknown;
    }
}

void ScribbleArea::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (mTabletInUse) { e->ignore(); return; }

    // Shift & Trace: double-click gizmo to reset properties
    if (mEditor->shiftTrace()->activeMoveEntryId() != -1)
    {
        auto stm = mEditor->shiftTrace();
        int id = stm->activeMoveEntryId();
        ShiftTraceManager::Entry entry;
        bool found = false;
        for (const auto& eEntry : stm->entries()) { if (eEntry.id == id) { entry = eEntry; found = true; break; } }
        if (found)
        {
            const QPointF clickScreen = e->localPos();

            // Build the same canvas-space transform the painter uses
            const qreal s    = qMax<qreal>(0.01, qIsFinite(entry.scale) ? entry.scale : 1.0);
            const qreal rDeg = qIsFinite(entry.rotationDeg) ? entry.rotationDeg : 0.0;
            QTransform canvasT;
            canvasT.translate(entry.pivot.x(), entry.pivot.y());
            if (!qFuzzyIsNull(rDeg))    canvasT.rotate(rDeg);
            if (!qFuzzyCompare(s, 1.0)) canvasT.scale(s, s);
            canvasT.translate(-entry.pivot.x(), -entry.pivot.y());
            canvasT.translate(entry.offset.x(), entry.offset.y());
            const QTransform fullT = canvasT * mEditor->view()->getView();

            QVector<QPointF> sc;
            QRectF canvasBounds;
            Layer* layer = mEditor->layers()->getLayer(entry.layerIndex);
            if (layer && layer->type() == Layer::BITMAP) {
                LayerBitmap* bl = static_cast<LayerBitmap*>(layer);
                canvasBounds = bl->getFrameBounds(entry.frameNumber);
            } else if (layer && layer->type() == Layer::VECTOR) {
                LayerVector* vl = static_cast<LayerVector*>(layer);
                VectorImage* vimg = vl->getVectorImageAtFrame(entry.frameNumber);
                if (vimg) {
                    QRectF r = vimg->getBoundsOfTransformedCurves();
                    if (r.isEmpty()) r = QRectF(QPointF(0,0), vimg->getSize());
                    canvasBounds = r;
                }
            }
            if (!canvasBounds.isEmpty()) {
                sc.resize(4);
                sc[0] = fullT.map(canvasBounds.topLeft());
                sc[1] = fullT.map(canvasBounds.topRight());
                sc[2] = fullT.map(canvasBounds.bottomRight());
                sc[3] = fullT.map(canvasBounds.bottomLeft());
            }

            if (sc.size() == 4) {
                const qreal handleR = 10.0;
                const QPointF moveCenter = (sc[0] + sc[1] + sc[2] + sc[3]) * 0.25;
                const QPointF rotCenter  = (sc[1] + sc[2]) * 0.5;
                const QRectF  moveRect(moveCenter.x() - handleR, moveCenter.y() - handleR, handleR * 2, handleR * 2);
                const QRectF  rotRect(rotCenter.x()  - handleR, rotCenter.y()  - handleR, handleR * 2, handleR * 2);

                // Rotation handle reset
                if (rotRect.contains(clickScreen)) {
                    stm->setEntryRotation(id, 0.0);
                    e->accept();
                    return;
                }
                // Scale handle reset
                for (const QPointF& cs : sc) {
                    if (QLineF(cs, clickScreen).length() <= handleR) {
                        stm->setEntryScale(id, 1.0);
                        e->accept();
                        return;
                    }
                }
                // Move handle reset
                if (moveRect.contains(clickScreen)) {
                    stm->setEntryPivotAndOffset(id, entry.defaultPivot, QPointF(0,0));
                    e->accept();
                    return;
                }
            }
        }
    }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->localPos()));
    currentTool()->pointerDoubleClickEvent(&event);
}

void ScribbleArea::showLayerNotVisibleWarning()
{
    QMessageBox::warning(this, tr("Warning"),
                          tr("You are trying to modify a hidden layer! Please select another layer (or make the current layer visible)."),
                          QMessageBox::Ok);
}

void ScribbleArea::paintBitmapBuffer()
{
    // Reset progressive commit state to prevent double-committing tiles
    mTiledBuffer.resetProgressiveCommitState();
    
    LayerBitmap* layer = static_cast<LayerBitmap*>(mEditor->layers()->currentLayer());
    Q_ASSERT(layer);
    Q_ASSERT(layer->type() == Layer::BITMAP);

    int frameNumber = mEditor->currentFrame();

    // If there is no keyframe at or before the current position,
    // just return (since we have nothing to paint on).
    if (layer->getLastKeyFrameAtPosition(frameNumber) == nullptr)
    {
        updateFrame();
        return;
    }

    BitmapImage* targetImage = currentBitmapImage(layer);
    if (targetImage != nullptr)
    {
        // Guard: if buffer is empty, nothing to paste (prevents double-commit)
        if (mTiledBuffer.bounds().isEmpty()) {
            return;
        }
        
        // Use SourceOver to blend tiles onto the bitmap. The overlay already uses
        // Source internally for dab composition within tiles to avoid self-darkening.
        QPainter::CompositionMode cm = QPainter::CompositionMode_SourceOver;
        switch (currentTool()->type())
        {
        case ERASER:
            cm = QPainter::CompositionMode_DestinationOut;
            break;
        default: //nothing
            break;
        }
        // If draw-behind is enabled (for bitmap tools), composite behind existing pixels
        if (currentTool()->properties.drawBehind && cm != QPainter::CompositionMode_DestinationOut)
        {
            cm = QPainter::CompositionMode_DestinationOver;
        }
        // Ensure all tiles are synced to pixmaps before paste
        {
            auto tiles = mTiledBuffer.tiles();
            for (auto it = tiles.begin(); it != tiles.end(); ++it) {
                Tile* t = it.value();
                if (t) t->syncPixmapFromImage();
            }
        }
        
        // Single paste: commit tiles to bitmap exactly once
        targetImage->paste(&mTiledBuffer, cm);
        layer->setModified(frameNumber, true);
    }

    QRect rect = mEditor->view()->mapCanvasToScreen(mTiledBuffer.bounds()).toRect();
    
    // Clear buffer immediately after paste to prevent any second paste
    mTiledBuffer.clear();
    
    update(rect);
}

void ScribbleArea::commitCurrentSegmentToLayer()
{
    LayerBitmap* layer = static_cast<LayerBitmap*>(mEditor->layers()->currentLayer());
    Q_ASSERT(layer);
    Q_ASSERT(layer->type() == Layer::BITMAP);

    int frameNumber = mEditor->currentFrame();

    if (layer->getLastKeyFrameAtPosition(frameNumber) == nullptr) {
        return;
    }

    BitmapImage* targetImage = currentBitmapImage(layer);
    if (targetImage != nullptr) {
        // Use SourceOver to blend tiles onto the bitmap. The overlay already uses
        // Source internally for dab composition within tiles to avoid self-darkening.
        QPainter::CompositionMode cm = QPainter::CompositionMode_SourceOver;
        if (currentTool()->type() == ERASER) {
            cm = QPainter::CompositionMode_DestinationOut;
        }
        if (currentTool()->properties.drawBehind && cm != QPainter::CompositionMode_DestinationOut) cm = QPainter::CompositionMode_DestinationOver;

        // Safety checks: if no dirty tiles or buffer bounds empty, nothing to commit
        if (mTiledBuffer.bounds().isEmpty()) return;
        const auto dirtyIndices = mTiledBuffer.mDirtyTiles;
        if (dirtyIndices.isEmpty()) return;

        // Sync only dirty tiles to pixmaps/images before paste (optimization)
        // This avoids syncing unchanged tiles, making commits much faster
        for (const TileIndex& idx : dirtyIndices) {
            Tile* t = mTiledBuffer.getTileFromIndex(idx, false);
            if (t) t->syncPixmapFromImage();
        }

        targetImage->paste(&mTiledBuffer, cm);
        layer->setModified(frameNumber, true);

        // Clear the tiled buffer after pasting. Frame update is triggered by the caller.
        mTiledBuffer.clear();
    }
}

void ScribbleArea::clearDrawingBuffer()
{
    mTiledBuffer.clear();
}

void ScribbleArea::clearOverlayRect(const QRectF& canvasRect)
{
    if (canvasRect.isEmpty()) return;
    QPainterPath p;
    p.addRect(canvasRect);
    mTiledBuffer.drawPath(p, QPen(Qt::NoPen), QBrush(QColor(0, 0, 0, 255)), QPainter::CompositionMode_Clear, false);
    updateCanvasRegion(canvasRect);
}

QRectF ScribbleArea::drawingBufferBounds() const
{
    return mTiledBuffer.bounds();
}

void ScribbleArea::handleDrawingOnEmptyFrame()
{
    auto layer = mEditor->layers()->currentLayer();

    if (!layer || !layer->isPaintable())
    {
        return;
    }

    if (currentTool()->type() == ERASER) {
        return;
    }

    int frameNumber = mEditor->currentFrame();
    if (layer->getKeyFrameAt(frameNumber)) { return; }

    // Drawing on an empty frame; take action based on preference.
    int action = mPrefs->getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION);
    auto previousKeyFrame = layer->getLastKeyFrameAtPosition(frameNumber);
    switch (action)
    {
    case KEEP_DRAWING_ON_PREVIOUS_KEY:
    {
        if (previousKeyFrame == nullptr) {
            mEditor->addNewKey();
        } else {
            onFrameModified(previousKeyFrame->pos());
        }
        break;
    }
    case DUPLICATE_PREVIOUS_KEY:
    {
        if (previousKeyFrame != nullptr) {
            KeyFrame* dupKey = previousKeyFrame->clone();
            layer->addKeyFrame(frameNumber, dupKey);
            mEditor->scrubTo(frameNumber);
            break;
        }
    }
    // if the previous keyframe doesn't exist,
    // an empty keyframe needs to be created, so
    // fallthrough
    case CREATE_NEW_KEY:
        mEditor->addNewKey();

        // Refresh canvas
        drawCanvas(frameNumber, mCanvas.rect());
        break;
    default:
        break;
    }
}

void ScribbleArea::paintEvent(QPaintEvent* event)
{
    int currentFrame = mEditor->currentFrame();
    
    // Fast navigation: flatten was built in startNavigating(). Draw background
    // via camera painter, then blit the flat image on top with the view transform.
    if (mIsNavigating && !currentTool()->isActive())
    {
        if (mFastNavCacheValid && !mFastNavFlat.isNull()) {
            // Camera painter handles the outside area (background, border).
            // It paints into mCanvas — clear it first to avoid accumulation.
            mCanvas.fill(Qt::transparent);
            prepCameraPainter(currentFrame);
            mCameraPainter.resetCache();
            mCameraPainter.paintCached(event->rect());

            // Blit flat image on top through the current view transform.
            // Use nearest-neighbour during active navigation — it's faster than
            // bilinear and the quality difference is imperceptible while moving.
            QPainter p(&mCanvas);
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.setWorldMatrixEnabled(true);
            p.setWorldTransform(mEditor->view()->getView());
            p.drawImage(mFastNavFlatOrigin, mFastNavFlat);
            p.end();
            // fall through to painter.drawPixmap(mCanvas) below
        } else {
            prepCanvas(currentFrame);
            prepCameraPainter(currentFrame);
            mCanvasPainter.paintFastPreview(event->rect());
            mCameraPainter.paintCached(event->rect());
        }
    }    else if (!currentTool()->isActive())
    {
        // --- we retrieve the canvas from the cache; we create it if it doesn't exist
        const int frameNumber = mEditor->layers()->lastFrameAtFrame(currentFrame);

        if (frameNumber < 0)
        {
            drawCanvas(currentFrame, event->rect());
        }
        else
        {
            // Cache key is derived from the underlying keyframe index and
            // current DimInactiveLayers state so we can reuse both dim-on
            // and dim-off variants without recompositing.
            const bool dimOn = mPrefs && mPrefs->isOn(SETTING::DIM_INACTIVE_LAYERS);
            const unsigned int baseIndex = static_cast<unsigned int>(frameNumber) * 2u;
            const unsigned int cacheIndex = baseIndex + (dimOn ? 1u : 0u);

            auto cacheKeyIter = mPixmapCacheKeys.find(cacheIndex);

            if (cacheKeyIter == mPixmapCacheKeys.end() || !QPixmapCache::find(cacheKeyIter.value(), &mCanvas))
            {
                drawCanvas(currentFrame, event->rect());
                // Cache by frame + dimming-state index so all exposures of the
                // same keyframe share both dimmed and non-dimmed variants.
                mPixmapCacheKeys[cacheIndex] = QPixmapCache::insert(mCanvas);
            }
            else
            {
                // Simply use the cached canvas from PixmapCache
            }
        }
    }
    else
    {
        prepCanvas(currentFrame);
        prepCameraPainter(currentFrame);
        prepOverlays(currentFrame);

        mCanvasPainter.paintCached(event->rect());
        mCameraPainter.paintCached(event->rect());
        // Draw Shift/Trace gizmo last, directly on the canvas pixmap
        mCanvasPainter.paintShiftTraceGizmoOnCanvas(event->rect());
    }

    if (currentTool()->type() == MOVE)
    {
        Layer* layer = mEditor->layers()->currentLayer();
        Q_CHECK_PTR(layer);
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = currentVectorImage(layer);
            if (vectorImage != nullptr)
            {
                vectorImage->setModified(true);
            }
        }
    }

    QPainter painter(this);

    // paints the canvas
    painter.setWorldMatrixEnabled(false);

    // In other places we use the blitRect to paint the buffer pixmap, however
    // the main pixmap which needs to be scaled accordingly to DPI, which is not accounted for when using the event rect
    // instead we can set a clipRect to avoid the area being updated needlessly
    painter.setClipRect(event->rect());

    // Render canvas normally; no special Gap Check recolor or overlays
    {
        const bool gapCheckOn = mEditor->preference()->isOn(SETTING::GAP_CHECK_MODE);
        if (gapCheckOn) {
            // Draw configured Gap Check background
            const int bgRgba = mEditor->preference()->getInt(SETTING::GAP_CHECK_BG_COLOR);
            QColor gapBg = QColor::fromRgba(bgRgba);
            painter.fillRect(event->rect(), gapBg);

            // Draw current canvas re-tinted to medium grey (preserve alpha)
            QImage grey = mCanvas.toImage();
            if (!grey.isNull()) {
                const int drawRgba = mEditor->preference()->getInt(SETTING::GAP_CHECK_DRAW_COLOR);
                const QColor drawCol = QColor::fromRgba(drawRgba).isValid() ? QColor::fromRgba(drawRgba) : QColor(211,211,211);
                const int dr = drawCol.red();
                const int dg = drawCol.green();
                const int db = drawCol.blue();
                for (int y = 0; y < grey.height(); ++y) {
                    QRgb* scan = reinterpret_cast<QRgb*>(grey.scanLine(y));
                    for (int x = 0; x < grey.width(); ++x) {
                        QRgb px = scan[x];
                        int a = qAlpha(px);
                        if (a == 0) continue;
                        scan[x] = qRgba(dr, dg, db, a);
                    }
                }
                painter.drawImage(QPointF(0,0), grey);
            }
        } else {
            painter.drawPixmap(QPointF(), mCanvas);
        }
    }

    // Draw bitmap hybrid vector stroke overlays (blue paths + red intersection dots)
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer && layer->type() == Layer::BITMAP) {
            // Hybrid stroke data is kept on BitmapImage for snapping and other
            // tools, but we intentionally do not render the vector overlays
            // here to avoid visual clutter in the main canvas view.
            Q_UNUSED(layer);
        }
    }

    // Draw floating selection content (lasso move/scale preview) always when hasFloating
    {
        auto sel = mEditor->select();
        if (!mCanvasInteractionLocked && sel->hasFloating() && !(mPlaybackPresentationModeEnabled && mEditor && mEditor->playback() && mEditor->playback()->isPlaying())) {
            painter.save();

            const bool smooth = sel->resampleSmooth();
            const QTransform S = sel->selectionTransform();
            const QPointF topLeft = sel->floatingOrigin() + sel->floatingOffset();

            if (!smooth) {
                // Non-destructive pixel-snap preview: render the rotated/scaled result
                // into a canvas-pixel-aligned buffer each frame so the overlay shows
                // exactly what the committed pixels will look like.  The source
                // mFloatingImage is never modified here.
                const QImage& src = sel->floatingImage();
                QRectF srcCanvasRect(topLeft, src.size());
                QPolygonF mappedPoly = S.isIdentity() ? QPolygonF(srcCanvasRect)
                                                      : S.map(QPolygonF(srcCanvasRect));
                QRect destCanvasRect = mappedPoly.boundingRect().toAlignedRect();

                if (!destCanvasRect.isEmpty()) {
                    QImage snapped(destCanvasRect.size(), QImage::Format_ARGB32_Premultiplied);
                    snapped.fill(Qt::transparent);
                    {
                        QPainter sp(&snapped);
                        sp.setRenderHint(QPainter::Antialiasing, false);
                        sp.setRenderHint(QPainter::SmoothPixmapTransform, false);
                        sp.translate(-destCanvasRect.topLeft());
                        if (!S.isIdentity()) {
                            sp.setWorldTransform(S, true);
                        }
                        sp.drawPixmap(topLeft.toPoint(), QPixmap::fromImage(src));
                        sp.end();
                    }
                    // Draw the pre-snapped image through the view transform only
                    painter.setWorldTransform(mEditor->view()->getView());
                    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
                    painter.setRenderHint(QPainter::Antialiasing, false);
                    painter.drawImage(destCanvasRect.topLeft(), snapped);
                }
            } else {
                // Smooth mode: let Qt apply the full transform with bilinear interpolation
                QTransform combined = S.isIdentity() ? mEditor->view()->getView()
                                                     : (S * mEditor->view()->getView());
                painter.setWorldTransform(combined);
                painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                painter.setRenderHint(QPainter::Antialiasing, false);
                const QPixmap overlayPix = QPixmap::fromImage(sel->floatingImage());
                painter.drawPixmap(topLeft, overlayPix);
            }

            painter.restore();
        }
    }

    // Paint tool vector overlay (e.g., curve preview paths with tapering)
    if (!mCanvasInteractionLocked && !(mPlaybackPresentationModeEnabled && mEditor && mEditor->playback() && mEditor->playback()->isPlaying()))
    {
        painter.save();
        painter.setWorldTransform(mEditor->view()->getView());
        painter.setRenderHint(QPainter::Antialiasing, true);
        currentTool()->paintOverlay(painter);
        painter.restore();

        currentTool()->paint(painter, event->rect());
    }

    if (!editor()->playback()->isPlaying())    // we don't need to display the following when the animation is playing
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = currentVectorImage(layer);
            if (vectorImage != nullptr)
            {
                switch (currentTool()->type())
                {
                case HAND:
                {
                    auto selectMan = mEditor->select();
                    painter.save();
                    painter.setWorldMatrixEnabled(false);
                    painter.setRenderHint(QPainter::Antialiasing, false);
                    // ----- paints the edited elements
                    QPen pen2(Qt::black, 0.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                    painter.setPen(pen2);
                    QColor color;
                    // ------------ vertices of the edited curves
                    color = QColor(200, 200, 200);
                    painter.setBrush(color);
                    VectorSelection vectorSelection = selectMan->vectorSelection;
                    for (int k = 0; k < vectorSelection.curve.size(); k++)
                    {
                        int curveNumber = vectorSelection.curve.at(k);

                        for (int vertexNumber = -1; vertexNumber < vectorImage->getCurveSize(curveNumber); vertexNumber++)
                        {
                            QPointF vertexPoint = vectorImage->getVertex(curveNumber, vertexNumber);
                            QRectF rectangle(mEditor->view()->mapCanvasToScreen(vertexPoint) - QPointF(3.0, 3.0), QSizeF(7, 7));
                            if (rect().contains(mEditor->view()->mapCanvasToScreen(vertexPoint).toPoint()))
                            {
                                painter.drawRect(rectangle);
                            }
                        }
                    }
                    // ------------ selected vertices of the edited curves
                    color = QColor(100, 100, 255);
                    painter.setBrush(color);
                    for (int k = 0; k < vectorSelection.vertex.size(); k++)
                    {
                        VertexRef vertexRef = vectorSelection.vertex.at(k);
                        QPointF vertexPoint = vectorImage->getVertex(vertexRef);
                        QRectF rectangle0 = QRectF(mEditor->view()->mapCanvasToScreen(vertexPoint) - QPointF(3.0, 3.0), QSizeF(7, 7));
                        painter.drawRect(rectangle0);
                    }
                    // ----- paints the closest vertices
                    color = QColor(255, 0, 0);
                    painter.setBrush(color);
                    QList<VertexRef> closestVertices = selectMan->closestVertices();
                    if (vectorSelection.curve.size() > 0)
                    {
                        for (int k = 0; k < closestVertices.size(); k++)
                        {
                            VertexRef vertexRef = closestVertices.at(k);
                            QPointF vertexPoint = vectorImage->getVertex(vertexRef);

                            QRectF rectangle = QRectF(mEditor->view()->mapCanvasToScreen(vertexPoint) - QPointF(3.0, 3.0), QSizeF(7, 7));
                            painter.drawRect(rectangle);
                        }
                    }
                    painter.restore();
                    break;
                }
                default:
                {
                    break;
                }
                } // end switch
            }
        }

        if (!(mPlaybackPresentationModeEnabled && mEditor && mEditor->playback() && mEditor->playback()->isPlaying())) {
            prepOverlays(currentFrame);
            mOverlayPainter.paint(painter, rect());
        }
    }
    
    // Draw selection overlay (marching ants and handles) — painted AFTER all
    // canvas content, floating pixels, tool overlays and grid so the gizmo
    // is always on top of everything.
    {
        if (!mCanvasInteractionLocked && !(mPlaybackPresentationModeEnabled && mEditor && mEditor->playback() && mEditor->playback()->isPlaying()))
        {
            auto sel = mEditor->select();
            if (sel->somethingSelected()) {
                TransformParameters tParams;
                tParams.originalSelectionRectF = sel->mySelectionRect();
                tParams.viewTransform = mEditor->view()->getView();
                tParams.selectionTransform = sel->selectionTransform();
                tParams.selectionPath = sel->selectionPathWithOffsets();
                // While creating lasso: draw path; after that: draw bounding box
                tParams.drawBoundingBoxForPath = (sel->state() != SelectionManager::SelectionState::CREATING);
                mSelectionPainter.paint(painter, mEditor->object(), mEditor->layers()->currentLayerIndex(), currentTool(), tParams);
            }
        }
    }

    // Draw progress bar for long operations (e.g., B-spline commit)
    if (mShowProgress) {
        const int barWidth = 200;
        const int barHeight = 20;
        const int centerX = width() / 2;
        const int centerY = height() / 2;
        
        QRect barRect(centerX - barWidth/2, centerY - barHeight/2, barWidth, barHeight);
        QRect fillRect(barRect.x(), barRect.y(), barWidth * mProgressValue, barHeight);
        
        // Semi-transparent dark background
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.fillRect(barRect.adjusted(-4, -4, 4, 4), QColor(0, 0, 0, 180));
        
        // Progress bar border
        painter.setPen(QColor(200, 200, 200));
        painter.drawRect(barRect);
        
        // Progress fill (cyan-ish)
        painter.fillRect(fillRect, QColor(100, 180, 255));
        
        // Optional label
        if (!mProgressLabel.isEmpty()) {
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(barRect.adjusted(0, -20, 0, 0), Qt::AlignCenter, mProgressLabel);
        }
    }

    // outlines the frame of the viewport
#ifdef _DEBUG
    painter.setWorldMatrixEnabled(false);
    painter.setPen(QPen(Qt::gray, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(0, 0, width(), height()));
#endif

    event->accept();
}

void ScribbleArea::paintSelectionVisuals(QPainter &painter)
{
    Object* object = mEditor->object();

    auto sel = mEditor->select();

    if (sel->selectionPathRaw().isEmpty()) { return; }

    // Draw floating content if present (preview while moving)
    if (sel->hasFloating() && (!qFuzzyIsNull(sel->floatingOffset().x()) || !qFuzzyIsNull(sel->floatingOffset().y()))) {
        painter.save();
        painter.setWorldTransform(editor()->view()->getView());
        QPointF topLeft = sel->floatingOrigin() + sel->floatingOffset();
        painter.drawImage(topLeft, sel->floatingImage());
        painter.restore();
    }

    // Draw selection outline (marching ants) with offsets
    QPainterPath path = sel->selectionPathWithOffsets();
    QPainterPath projectedPath = editor()->view()->getView().map(path);

    painter.setBrush(Qt::NoBrush);


    QVector<qreal> dash = { 4.0, 4.0 };
    QPen blackPen(Qt::black, 1.0, Qt::DashLine);
    blackPen.setDashPattern(dash);
    static int antsPhase = 0;
    blackPen.setDashOffset(antsPhase * 0.5);
    QPen whitePen(Qt::white, 1.0, Qt::DashLine);
    whitePen.setDashPattern(dash);
    whitePen.setDashOffset(antsPhase * 0.5 + 4.0);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(blackPen);
    painter.drawPath(projectedPath);
    painter.setPen(whitePen);
    painter.drawPath(projectedPath);

    antsPhase = (antsPhase + 1) % 1000;

    emit selectionUpdated();
}

BitmapImage* ScribbleArea::currentBitmapImage(Layer* layer)
{
    Q_ASSERT(layer->type() == Layer::BITMAP);
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);
    const int cf = mEditor->currentFrame();

    // Resolve to the keyframe that covers the current frame
    KeyFrame* kf = bitmapLayer->keyExists(cf)
                   ? bitmapLayer->getKeyFrameAt(cf)
                   : bitmapLayer->getLastKeyFrameAtPosition(cf);
    if (!kf) return nullptr;

    BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
    // Return the active inner layer's image — all drawing tools write here,
    // not to the composite. CanvasPainter uses getCell() (composite) separately.
    return bitmapLayer->cellBank()->getActiveInnerLayerImage(bkf->cellId());
}

VectorImage* ScribbleArea::currentVectorImage(Layer* layer) const
{
    Q_ASSERT(layer->type() == Layer::VECTOR);
    auto vectorLayer = static_cast<LayerVector*>(layer);
    const int cf = mEditor->currentFrame();
    // Prefer exact current keyframe when present
    if (vectorLayer->keyExists(cf)) {
        return vectorLayer->getVectorImageAtFrame(cf);
    }
    // Fallback to last vector image at or before
    return vectorLayer->getLastVectorImageAtFrame(cf, 0);
}

void ScribbleArea::prepCameraPainter(int frame)
{
    Object* object = mEditor->object();

    const int canvasRgba = mPrefs->getInt(SETTING::CANVAS_COLOR);
    mCameraPainter.setCanvasColorRgba(canvasRgba);

    const int extraStyle = mPrefs->getInt(SETTING::EXTRA_SPACE_STYLE);
    const int extraScale = mPrefs->getInt(SETTING::EXTRA_SPACE_SCALE_PERCENT);
    const int extraW = mPrefs->getInt(SETTING::EXTRA_SPACE_MANUAL_W);
    const int extraH = mPrefs->getInt(SETTING::EXTRA_SPACE_MANUAL_H);
    mCameraPainter.setExtraSpaceSettings(extraStyle, extraScale, extraW, extraH);

    mCameraPainter.preparePainter(object,
                                  mEditor->currentLayerIndex(),
                                  frame,
                                  mEditor->view()->getView(),
                                  mEditor->playback()->isPlaying(),
                                  mLayerVisibility,
                                  mPrefs->getFloat(SETTING::LAYER_VISIBILITY_THRESHOLD),
                                  mPrefs->isOn(SETTING::DIM_INACTIVE_LAYERS),
                                  mEditor->view()->getScaleInversed());

    OnionSkinPainterOptions onionSkinOptions;
    onionSkinOptions.enabledWhilePlaying = mPrefs->getInt(SETTING::ONION_WHILE_PLAYBACK);
    onionSkinOptions.isPlaying = mEditor->playback()->isPlaying();
    onionSkinOptions.isAbsolute = (mPrefs->getString(SETTING::ONION_TYPE) == "absolute");
    onionSkinOptions.skinPrevFrames = !mEditor->shiftTrace() && mPrefs->isOn(SETTING::PREV_ONION);
    onionSkinOptions.skinNextFrames = !mEditor->shiftTrace() && mPrefs->isOn(SETTING::NEXT_ONION);
    onionSkinOptions.colorizePrevFrames = mPrefs->isOn(SETTING::ONION_RED);
    onionSkinOptions.colorizeNextFrames = mPrefs->isOn(SETTING::ONION_BLUE);
    onionSkinOptions.framesToSkinPrev = mPrefs->getInt(SETTING::ONION_PREV_FRAMES_NUM);
    onionSkinOptions.framesToSkinNext = mPrefs->getInt(SETTING::ONION_NEXT_FRAMES_NUM);
    onionSkinOptions.maxOpacity = mPrefs->getInt(SETTING::ONION_MAX_OPACITY);
    onionSkinOptions.minOpacity = mPrefs->getInt(SETTING::ONION_MIN_OPACITY);

    if (mPlaybackPresentationModeEnabled && mEditor && mEditor->playback() && mEditor->playback()->isPlaying()) {
        onionSkinOptions.enabledWhilePlaying = false;
        onionSkinOptions.skinPrevFrames = false;
        onionSkinOptions.skinNextFrames = false;
    }

    mCameraPainter.setOnionSkinPainterOptions(onionSkinOptions);
}

void ScribbleArea::prepCanvas(int frame)
{
    Object* object = mEditor->object();

    CanvasPainterOptions o;
    o.bOnionSkinMultiLayer = mPrefs->isOn(SETTING::ONION_MUTLIPLE_LAYERS);
    o.bAntiAlias = mPrefs->isOn(SETTING::ANTIALIAS);
    o.bThinLines = mPrefs->isOn(SETTING::INVISIBLE_LINES);
    o.bOutlines = mPrefs->isOn(SETTING::OUTLINES);
    o.eLayerVisibility = mLayerVisibility;
    o.fLayerVisibilityThreshold = mPrefs->getFloat(SETTING::LAYER_VISIBILITY_THRESHOLD);
    o.bDimInactiveLayers = mPrefs->isOn(SETTING::DIM_INACTIVE_LAYERS);
    o.fDimInactiveOpacity = 0.25f; // fixed factor for now; could be made configurable later
    o.scaling = mEditor->view()->scaling();
    o.cmBufferBlendMode = mEditor->tools()->currentTool()->type() == ToolType::ERASER ? QPainter::CompositionMode_DestinationOut : QPainter::CompositionMode_SourceOver;

    OnionSkinPainterOptions onionSkinOptions;
    onionSkinOptions.enabledWhilePlaying = mPrefs->getInt(SETTING::ONION_WHILE_PLAYBACK);
    onionSkinOptions.isPlaying = mEditor->playback()->isPlaying();
    onionSkinOptions.isAbsolute = (mPrefs->getString(SETTING::ONION_TYPE) == "absolute");
    onionSkinOptions.skinPrevFrames = mPrefs->isOn(SETTING::PREV_ONION);
    onionSkinOptions.skinNextFrames = mPrefs->isOn(SETTING::NEXT_ONION);
    onionSkinOptions.colorizePrevFrames = mPrefs->isOn(SETTING::ONION_RED);
    onionSkinOptions.colorizeNextFrames = mPrefs->isOn(SETTING::ONION_BLUE);
    onionSkinOptions.framesToSkinPrev = mPrefs->getInt(SETTING::ONION_PREV_FRAMES_NUM);
    onionSkinOptions.framesToSkinNext = mPrefs->getInt(SETTING::ONION_NEXT_FRAMES_NUM);
    onionSkinOptions.maxOpacity = mPrefs->getInt(SETTING::ONION_MAX_OPACITY);
    onionSkinOptions.minOpacity = mPrefs->getInt(SETTING::ONION_MIN_OPACITY);

    if (mPlaybackPresentationModeEnabled && mEditor && mEditor->playback() && mEditor->playback()->isPlaying()) {
        onionSkinOptions.enabledWhilePlaying = false;
        onionSkinOptions.skinPrevFrames = false;
        onionSkinOptions.skinNextFrames = false;
    }

    mCanvasPainter.setOnionSkinOptions(onionSkinOptions);
    mCanvasPainter.setOptions(o);
    mCanvasPainter.setShiftTraceManager(mEditor->shiftTrace());

    ViewManager* vm = mEditor->view();
    SelectionManager* sm = mEditor->select();
    mCanvasPainter.setViewTransform(vm->getView(), vm->getViewInverse());
    // If we have a floating selection, let the overlay handle transform preview to honor AA mode.
    if (sm->hasFloating()) {
        mCanvasPainter.ignoreTransformedSelection();
    } else if (sm->somethingSelected() && !sm->selectionTransform().isIdentity()) {
        const QRect selRect = sm->mySelectionRect().toRect();
        if (!selRect.isEmpty()) {
            if (sm->hasSelectionPath()) {
                mCanvasPainter.setTransformedSelection(selRect, sm->selectionTransform(), sm->selectionPathWithOffsets());
            } else {
                mCanvasPainter.setTransformedSelection(selRect, sm->selectionTransform());
            }
        } else {
            mCanvasPainter.ignoreTransformedSelection();
        }
    } else {
        mCanvasPainter.ignoreTransformedSelection();
    }

    mCanvasPainter.setPaintSettings(object, mEditor->layers()->currentLayerIndex(), frame, &mTiledBuffer);
}

void ScribbleArea::drawCanvas(int frame, QRect rect)
{
    prepCanvas(frame);
    prepCameraPainter(frame);
    prepOverlays(frame);
    mCanvasPainter.paint(rect);
    mCameraPainter.paint(rect);
    // Draw Shift/Trace gizmo last, directly on the canvas pixmap
    mCanvasPainter.paintShiftTraceGizmoOnCanvas(rect);
}

void ScribbleArea::setGaussianGradient(QGradient &gradient, QColor color, qreal opacity, qreal offset)
{
    if (offset < 0) { offset = 0; }
    if (offset > 100) { offset = 100; }

    int r = color.red();
    int g = color.green();
    int b = color.blue();
    qreal a = color.alphaF();

    int mainColorAlpha = qRound(a * 255 * opacity);

    // the more feather (offset), the more softness (opacity)
    int alphaAdded = qRound((mainColorAlpha * offset) / 100);

    gradient.setColorAt(0.0, QColor(r, g, b, mainColorAlpha - alphaAdded));
    gradient.setColorAt(1.0, QColor(r, g, b, 0));
    gradient.setColorAt(1.0 - (offset / 100.0), QColor(r, g, b, mainColorAlpha - alphaAdded));
}

void ScribbleArea::drawPath(QPainterPath path, QPen pen, QBrush brush, QPainter::CompositionMode cm)
{
    mTiledBuffer.drawPath(mEditor->view()->mapScreenToCanvas(path), pen, brush, cm, mPrefs->isOn(SETTING::ANTIALIAS));
}

void ScribbleArea::drawPathCanvas(QPainterPath path, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool useAA)
{
    mTiledBuffer.drawPath(path, pen, brush, cm, useAA);
}

void ScribbleArea::drawPen(QPointF thePoint, qreal brushWidth, QColor fillColor, bool useAA)
{
    // Use Source to avoid dabs darkening each other within the overlay tiles.
    // The final paste to bitmap uses SourceOver to preserve underlying strokes.
    mTiledBuffer.drawBrush(thePoint, brushWidth, Qt::NoPen, QBrush(fillColor, Qt::SolidPattern), QPainter::CompositionMode_Source, useAA, true);
}


void ScribbleArea::drawBrush(QPointF thePoint, qreal brushWidth, qreal mOffset, QColor fillColor, QPainter::CompositionMode compMode, qreal opacity, bool usingFeather, bool useAA)
{
    QBrush brush;
    if (usingFeather)
    {
        QRadialGradient radialGrad(thePoint, 0.5 * brushWidth);
        setGaussianGradient(radialGrad, fillColor, opacity, mOffset);
        brush = radialGrad;
    }
    else
    {
        brush = QBrush(fillColor, Qt::SolidPattern);
    }
    mTiledBuffer.drawBrush(thePoint, brushWidth, Qt::NoPen, brush, compMode, useAA);
}

void ScribbleArea::drawImageOverlay(const QImage& image, const QRectF& imageBounds, QPainter::CompositionMode cm, bool useAA)
{
    if (image.isNull() || imageBounds.isEmpty()) return;
    // Stamp image into the overlay tiled buffer in canvas coordinates
    mTiledBuffer.drawImage(image, imageBounds.toAlignedRect(), cm, useAA);
    // Expand pending update region in canvas space and schedule a UI update
    QRectF padded = imageBounds.adjusted(-2.0, -2.0, 2.0, 2.0);
    updateCanvasRegion(padded);
}

void ScribbleArea::drawPolyline(QPainterPath path, QPen pen, bool useAA)
{
    BlitRect blitRect;

    // In order to clear what was previously dirty, we need to include the previous buffer bound
    // this ensures that we won't see stroke artifacts
    blitRect.extend(mEditor->view()->mapCanvasToScreen(mTiledBuffer.bounds()).toRect());

    QRect updateRect = mEditor->view()->mapCanvasToScreen(path.boundingRect()).toRect();
    // Now extend with the new path bounds mapped to the local coordinate
    blitRect.extend(updateRect);

    mTiledBuffer.clear();
    mTiledBuffer.drawPath(path, pen, Qt::NoBrush, QPainter::CompositionMode_SourceOver, useAA);

    // And update only the affected area
    update(blitRect.adjusted(-1, -1, 1, 1));
}

void ScribbleArea::drawTaperedCurve(const QPainterPath& path, QPen& pen, bool isCommit, qreal taperStartValue, qreal taperEndValue, bool useAA, int taperMode, std::function<void(qreal)> progressCallback)
{
    const qreal totalLength = path.length();
    const qreal fullWidth = pen.widthF();

    if (isCommit) {
        beginDrawingStroke(fullWidth);
    }

    if (!(totalLength >= 2.0)) {
        // Too short; fallback to regular polyline rendering
        drawPolyline(path, pen, useAA);
        return;
    }

    // Calculate taper lengths based on mode (0=FixedPixels, 1=Percentage)
    qreal startTaperLength = 0.0;
    qreal endTaperLength = 0.0;

    if (taperMode == 0) {  // FixedPixels
        // FIXED PIXELS MODE: Interpret values as pixel distances
        const qreal requestedStartTaperPixels = taperStartValue;
        const qreal requestedEndTaperPixels = taperEndValue;
        const qreal totalRequestedTaper = requestedStartTaperPixels + requestedEndTaperPixels;

        // Check if stroke is long enough for fixed pixel distances
        if (totalRequestedTaper > 0.0 && totalLength >= totalRequestedTaper) {
            // Stroke is long enough: use exact pixel distances
            startTaperLength = requestedStartTaperPixels;
            endTaperLength = requestedEndTaperPixels;
        } else if (totalRequestedTaper > 0.0) {
            // Stroke is too short: scale down proportionally
            const qreal startRatio = (totalRequestedTaper > 0.0) ? (requestedStartTaperPixels / totalRequestedTaper) : 0.5;
            const qreal endRatio = 1.0 - startRatio;
            startTaperLength = totalLength * startRatio;
            endTaperLength = totalLength * endRatio;
        }
    } else {  // Percentage mode (taperMode == 1)
        // PERCENTAGE MODE: Interpret values as percentages (0-100)
        const qreal startPercent = qBound(0.0, taperStartValue, 100.0) / 100.0;
        const qreal endPercent = qBound(0.0, taperEndValue, 100.0) / 100.0;
        const qreal totalPercent = startPercent + endPercent;

        // Cap total to 100% to prevent overlap
        if (totalPercent > 1.0) {
            const qreal scale = 1.0 / totalPercent;
            startTaperLength = totalLength * startPercent * scale;
            endTaperLength = totalLength * endPercent * scale;
        } else {
            startTaperLength = totalLength * startPercent;
            endTaperLength = totalLength * endPercent;
        }
    }

    const QColor strokeColor = pen.color();

    // Early-out optimization for negligible tapering: use fast polyline rendering
    if (startTaperLength < 1.0 && endTaperLength < 1.0) {
        drawPolyline(path, pen, useAA);
        return;
    }

    // PERFORMANCE OPTIMIZATION: Split stroke into 3 segments
    // 1. Tapered start (slow, dab-by-dab)
    // 2. Constant-width middle (FAST, single path stroke)
    // 3. Tapered end (slow, dab-by-dab)
    // This is a massive speedup because the middle is typically 80-90% of the stroke
    
    const qreal middleStart = startTaperLength;
    const qreal middleEnd = totalLength - endTaperLength;
    const bool hasConstantMiddle = (middleEnd > middleStart + 1.0);

    QRectF previousBufferBounds;
    if (!isCommit) {
        previousBufferBounds = drawingBufferBounds();
        clearDrawingBuffer();
    }

    // Adaptive sampling for tapered regions only
    const qreal targetSpacing = qMax<qreal>(1.0, 0.25 * fullWidth);
    const qreal minStep = 0.5;
    const int maxSamples = isCommit ? INT_MAX : 2000;

    // === SEGMENT 1: Tapered start (if any) ===
    if (startTaperLength > 0.0) {
        // Pre-calculate all dab positions and sizes
        struct DabData { QPointF pos; qreal width; };
        QVector<DabData> startDabs;
        
        qreal dist = 0.0;
        int sampleIndex = 0;
        while (dist <= startTaperLength && sampleIndex < maxSamples) {
            const qreal t = path.percentAtLength(dist);
            const QPointF point = path.pointAtPercent(t);
            const qreal ramp = qMin<qreal>(1.0, dist / startTaperLength);
            qreal width = 1.0 + (fullWidth - 1.0) * ramp;
            if (width < 1.0) width = 1.0;
            startDabs.append({point, width});
            const qreal step = qBound(minStep, 0.25 * width, 0.45 * fullWidth);
            dist += step;
            ++sampleIndex;
        }
        
        // Batch render all dabs at once (much faster than serial)
        for (const auto& dab : startDabs) {
            drawPen(dab.pos, dab.width, strokeColor, useAA);
        }
        
        if (progressCallback) progressCallback(0.33); // 33% done after start taper
    }

    // === SEGMENT 2: Constant-width middle (FAST PATH) ===
    if (hasConstantMiddle) {
        // Extract middle segment of path
        const qreal t1 = path.percentAtLength(middleStart);
        const qreal t2 = path.percentAtLength(middleEnd);
        
        QPainterPath middlePath;
        const int segments = qBound(2, int((middleEnd - middleStart) / 5.0), 500);
        bool firstPoint = true;
        
        for (int i = 0; i <= segments; ++i) {
            const qreal t = t1 + (t2 - t1) * (qreal(i) / qreal(segments));
            const QPointF p = path.pointAtPercent(t);
            if (firstPoint) {
                middlePath.moveTo(p);
                firstPoint = false;
            } else {
                middlePath.lineTo(p);
            }
        }
        
        // Use fast stroking for constant-width segment
        QPainterPathStroker stroker;
        stroker.setWidth(fullWidth);
        stroker.setCapStyle(Qt::FlatCap); // Flat caps to join with tapered segments
        stroker.setJoinStyle(Qt::RoundJoin);
        QPainterPath stroked = stroker.createStroke(middlePath);
        
        mTiledBuffer.drawPath(stroked, Qt::NoPen, QBrush(strokeColor), QPainter::CompositionMode_SourceOver, useAA);
        
        if (progressCallback) progressCallback(0.66); // 66% done after middle
    }

    // === SEGMENT 3: Tapered end (if any) ===
    if (endTaperLength > 0.0) {
        // Pre-calculate all dab positions and sizes
        struct DabData { QPointF pos; qreal width; };
        QVector<DabData> endDabs;
        
        qreal dist = middleEnd;
        int sampleIndex = 0;
        while (dist <= totalLength && sampleIndex < maxSamples) {
            const qreal t = path.percentAtLength(dist);
            const QPointF point = path.pointAtPercent(t);
            const qreal ramp = qMin<qreal>(1.0, (totalLength - dist) / endTaperLength);
            qreal width = 1.0 + (fullWidth - 1.0) * ramp;
            if (width < 1.0) width = 1.0;
            endDabs.append({point, width});
            const qreal step = qBound(minStep, 0.25 * width, 0.45 * fullWidth);
            dist += step;
            ++sampleIndex;
        }
        
        // Batch render all dabs at once (much faster than serial)
        for (const auto& dab : endDabs) {
            drawPen(dab.pos, dab.width, strokeColor, useAA);
        }
        
        if (progressCallback) progressCallback(1.0); // 100% done
    }

    if (!isCommit) {
        QRectF updateRegion = previousBufferBounds;
        updateRegion = updateRegion.united(path.boundingRect());
        const qreal padding = fullWidth * 1.5;
        updateRegion = updateRegion.adjusted(-padding, -padding, padding, padding);
        updateCanvasRegion(updateRegion);
    }
}

void ScribbleArea::beginDrawingStroke(qreal brushWidth)
{
    setStrokeActive(true);
    mTiledBuffer.beginStroke(brushWidth);
    resetProgressiveCommitTracking();
}

void ScribbleArea::flushDrawingStroke()
{
    mTiledBuffer.flushDabs();
    // Ensure accumulated canvas updates are visible after flushing dabs
    flushCanvasUpdates();
}

void ScribbleArea::endStroke()
{
    setStrokeActive(false);
    if (mEditor->layers()->currentLayer()->type() == Layer::BITMAP) {
        flushDrawingStroke();
        paintBitmapBuffer();
        
        // Trigger progressive commit cleanup after stroke completion
        // This commits any remaining tiles to the bitmap layer for memory management
        commitCompletedTiles();
    }

    onFrameModified(mEditor->currentFrame());
    resetProgressiveCommitTracking();
}

void ScribbleArea::flipSelection(bool flipVertical)
{
    mEditor->select()->flipSelection(flipVertical);
}

void ScribbleArea::prepOverlays(int frame)
{
    OverlayPainterOptions o;

    o.bGrid = mPrefs->isOn(SETTING::GRID);
    o.nGridSizeW = mPrefs->getInt(SETTING::GRID_SIZE_W);
    o.nGridSizeH = mPrefs->getInt(SETTING::GRID_SIZE_H);
    o.bCenter = mPrefs->isOn(SETTING::OVERLAY_CENTER);
    o.bThirds = mPrefs->isOn(SETTING::OVERLAY_THIRDS);
    o.bGoldenRatio = mPrefs->isOn(SETTING::OVERLAY_GOLDEN);
    o.bSafeArea = mPrefs->isOn(SETTING::OVERLAY_SAFE);
    o.bPerspective1 = mPrefs->isOn(SETTING::OVERLAY_PERSPECTIVE1);
    o.bPerspective2 = mPrefs->isOn(SETTING::OVERLAY_PERSPECTIVE2);
    o.bPerspective3 = mPrefs->isOn(SETTING::OVERLAY_PERSPECTIVE3);
    o.nOverlayAngle = mPrefs->getInt(SETTING::OVERLAY_ANGLE);
    o.bActionSafe = mPrefs->isOn(SETTING::ACTION_SAFE_ON);
    o.nActionSafe = mPrefs->getInt(SETTING::ACTION_SAFE);
    o.bShowSafeAreaHelperText = mPrefs->isOn(SETTING::OVERLAY_SAFE_HELPER_TEXT_ON);
    o.bTitleSafe = mPrefs->isOn(SETTING::TITLE_SAFE_ON);
    o.nTitleSafe = mPrefs->getInt(SETTING::TITLE_SAFE);
    o.nOverlayAngle = mPrefs->getInt(SETTING::OVERLAY_ANGLE);
    o.bShowHandle = mEditor->tools()->currentTool()->type() == MOVE && mEditor->layers()->currentLayer()->type() != Layer::CAMERA;

    o.mSinglePerspPoint = mEditor->overlays()->getSinglePerspectivePoint();
    o.mLeftPerspPoint = mEditor->overlays()->getLeftPerspectivePoint();
    o.mRightPerspPoint = mEditor->overlays()->getRightPerspectivePoint();
    o.mMiddlePerspPoint = mEditor->overlays()->getMiddlePerspectivePoint();

    o.nFrameIndex = frame;

    mOverlayPainter.setOptions(o);
    mOverlayPainter.preparePainter(mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex()), palette());

    ViewManager* vm = mEditor->view();
    mOverlayPainter.setViewTransform(vm->getView());
}

void ScribbleArea::blurBrush(BitmapImage *bmiSource_, QPointF srcPoint_, QPointF thePoint_, qreal brushWidth_, qreal mOffset_, qreal opacity_)
{
    QRadialGradient radialGrad(thePoint_, 0.5 * brushWidth_);
    setGaussianGradient(radialGrad, QColor(255, 255, 255, 127), opacity_, mOffset_);

    QRectF srcRect(srcPoint_.x() - 0.5 * brushWidth_, srcPoint_.y() - 0.5 * brushWidth_, brushWidth_, brushWidth_);
    QRectF trgRect(thePoint_.x() - 0.5 * brushWidth_, thePoint_.y() - 0.5 * brushWidth_, brushWidth_, brushWidth_);

    BitmapImage bmiSrcClip = bmiSource_->copy(srcRect.toAlignedRect());
    BitmapImage bmiTmpClip = bmiSrcClip; // TODO: find a shorter way

    bmiTmpClip.drawRect(srcRect, Qt::NoPen, radialGrad, QPainter::CompositionMode_Source, true);
    bmiSrcClip.bounds().moveTo(trgRect.topLeft().toPoint());
    bmiTmpClip.paste(&bmiSrcClip, QPainter::CompositionMode_SourceIn);
    mTiledBuffer.drawImage(*bmiTmpClip.image(), bmiTmpClip.bounds(), QPainter::CompositionMode_SourceOver, mPrefs->isOn(SETTING::ANTIALIAS));
}

void ScribbleArea::liquifyBrush(BitmapImage *bmiSource_, QPointF srcPoint_, QPointF thePoint_, qreal brushWidth_, qreal mOffset_, qreal opacity_)
{
    QPointF delta = (thePoint_ - srcPoint_); // increment vector
    QRectF trgRect(thePoint_.x() - 0.5 * brushWidth_, thePoint_.y() - 0.5 * brushWidth_, brushWidth_, brushWidth_);

    QRadialGradient radialGrad(thePoint_, 0.5 * brushWidth_);
    setGaussianGradient(radialGrad, QColor(255, 255, 255, 255), opacity_, mOffset_);

    // Create gradient brush
    BitmapImage bmiTmpClip;
    bmiTmpClip.drawRect(trgRect, Qt::NoPen, radialGrad, QPainter::CompositionMode_Source, mPrefs->isOn(SETTING::ANTIALIAS));

    // Slide texture/pixels of the source image
    qreal factor, factorGrad;

    for (int yb = bmiTmpClip.top(); yb < bmiTmpClip.bottom(); yb++)
    {
        for (int xb = bmiTmpClip.left(); xb < bmiTmpClip.right(); xb++)
        {
            QColor color;
            color.setRgba(bmiTmpClip.pixel(xb, yb));
            factorGrad = color.alphaF(); // any from r g b a is ok

            int xa = xb - factorGrad * delta.x();
            int ya = yb - factorGrad * delta.y();

            color.setRgba(bmiSource_->pixel(xa, ya));
            factor = color.alphaF();

            if (factor > 0.0)
            {
                color.setRed(color.red() / factor);
                color.setGreen(color.green() / factor);
                color.setBlue(color.blue() / factor);
                color.setAlpha(255); // Premultiplied color

                color.setRed(color.red()*factorGrad);
                color.setGreen(color.green()*factorGrad);
                color.setBlue(color.blue()*factorGrad);
                color.setAlpha(255 * factorGrad); // Premultiplied color

                bmiTmpClip.setPixel(xb, yb, color.rgba());
            }
            else
            {
                bmiTmpClip.setPixel(xb, yb, qRgba(255, 255, 255, 0));
            }
        }
    }
    mTiledBuffer.drawImage(*bmiTmpClip.image(), bmiTmpClip.bounds(), QPainter::CompositionMode_SourceOver, mPrefs->isOn(SETTING::ANTIALIAS));
}

/************************************************************************************/
// view handling

QPointF ScribbleArea::getCentralPoint()
{
    return mEditor->view()->mapScreenToCanvas(QPointF(width() / 2, height() / 2));
}

void ScribbleArea::applyTransformedSelection()
{
    mCanvasPainter.ignoreTransformedSelection();

    Layer* layer = mEditor->layers()->currentLayer();

    bool useAA = mEditor->tools()->currentTool()->properties.useAA;

    if (layer == nullptr) { return; }

    auto selectMan = mEditor->select();
    const bool needsFloatCommit = selectMan->needsFloatingCommit();
    if (selectMan->somethingSelected())
    {
        if (!selectMan->mySelectionRect().isEmpty() && !selectMan->selectionTransform().isIdentity())
        {

        if (layer->type() == Layer::BITMAP)
        {
            handleDrawingOnEmptyFrame();
            BitmapImage* bitmapImage = currentBitmapImage(layer);
            if (bitmapImage == nullptr) { return; }
            BitmapImage transformedImage = bitmapImage->transformed(selectMan->mySelectionRect().toRect(), selectMan->selectionTransform(), useAA);

            // If we have a lasso path, mask the transformed image and the clear area to the path
            if (!selectMan->selectionPath().isEmpty()) {
                QImage masked = *transformedImage.image();
                QPainter maskPainter(&masked);
                maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                // Create a mask in selection local space
                QImage pathMask(masked.size(), QImage::Format_ARGB32_Premultiplied);
                pathMask.fill(Qt::transparent);
                QPainter pathPainter(&pathMask);
                QPainterPath localPath = selectMan->selectionPath(); // already in canvas space
                localPath.translate(-selectMan->mySelectionRect().topLeft());
                pathPainter.fillPath(localPath, Qt::white);
                pathPainter.end();
                maskPainter.drawImage(QPoint(0,0), pathMask);
                maskPainter.end();
                transformedImage.setImage(new QImage(masked));

                // Clear only inside the path on the destination
                QPainter clearPainter(bitmapImage->image());
                clearPainter.setCompositionMode(QPainter::CompositionMode_Clear);
                QPainterPath worldPath = selectMan->selectionPath();
                // Translate canvas-space path to image-local coordinates
                QPainterPath imageLocalPath = worldPath.translated(-bitmapImage->topLeft());
                clearPainter.fillPath(imageLocalPath, Qt::white);
                clearPainter.end();
            } else {
                bitmapImage->clear(selectMan->mySelectionRect());
            }
            bitmapImage->paste(&transformedImage, QPainter::CompositionMode_SourceOver);
        }
        else if (layer->type() == Layer::VECTOR)
        {
            // Unfortunately this doesn't work right currently so vector transforms
            // will always be applied on the previous keyframe when on an empty frame
            //handleDrawingOnEmptyFrame();
            VectorImage* vectorImage = currentVectorImage(layer);
            if (vectorImage == nullptr) { return; }

            vectorImage->applySelectionTransformation();
        }

            mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());

            // Bake transform into selection geometry so outline stays in place
            selectMan->bakeTransformIntoSelectionGeometry();
        }
    }

    // Commit floating move for lasso selections if pending and originating
    // from a cut/move. For floating selections created by paste, we defer the
    // commit to Editor::deselectAll() so it can also create the corresponding
    // undo/redo entry at commit time.
    if (needsFloatCommit && !selectMan->isFloatingFromPaste()) {
        handleDrawingOnEmptyFrame();
        selectMan->commitFloatingMove();
        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    }

    updateFrame();
}

void ScribbleArea::cancelTransformedSelection()
{
    mCanvasPainter.ignoreTransformedSelection();

    auto selectMan = mEditor->select();
    if (selectMan->somethingSelected())
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer == nullptr) { return; }

        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = currentVectorImage(layer);
            if (vectorImage != nullptr)
            {
                vectorImage->setSelectionTransformation(QTransform());
            }
        }

        mEditor->select()->setSelection(selectMan->mySelectionRect(), false);

        selectMan->resetSelectionProperties();
        mOriginalPolygonF = QPolygonF();

        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
        updateFrame();
    }

    // Discard floating selection if present (lasso move)
    if (selectMan->hasFloating()) {
        selectMan->clearAll();
        updateFrame();
    }
}

void ScribbleArea::toggleThinLines()
{
    bool previousValue = mPrefs->isOn(SETTING::INVISIBLE_LINES);
    setEffect(SETTING::INVISIBLE_LINES, !previousValue);
}

void ScribbleArea::setLayerVisibility(LayerVisibility visibility)
{
    mLayerVisibility = visibility;
    mPrefs->set(SETTING::LAYER_VISIBILITY, static_cast<int>(mLayerVisibility));

    invalidateAllCache();
}

void ScribbleArea::increaseLayerVisibilityIndex()
{
    ++mLayerVisibility;
    mPrefs->set(SETTING::LAYER_VISIBILITY, static_cast<int>(mLayerVisibility));

    invalidateAllCache();
}

void ScribbleArea::decreaseLayerVisibilityIndex()
{
    --mLayerVisibility;
    mPrefs->set(SETTING::LAYER_VISIBILITY, static_cast<int>(mLayerVisibility));

    invalidateAllCache();
}

/************************************************************************************/
// tool handling

BaseTool* ScribbleArea::currentTool() const
{
    return editor()->tools()->currentTool();
}

void ScribbleArea::deleteSelection()
{
    auto selectMan = mEditor->select();
    if (selectMan->somethingSelected())      // there is something selected
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer == nullptr) { return; }

        handleDrawingOnEmptyFrame();

        const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);

        selectMan->clearCurves();
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = currentVectorImage(layer);
            Q_CHECK_PTR(vectorImage);
            vectorImage->deleteSelection();
        }
        else if (layer->type() == Layer::BITMAP)
        {
            BitmapImage* bitmapImage = currentBitmapImage(layer);
            Q_CHECK_PTR(bitmapImage);

            if (!selectMan->selectionPathRaw().isEmpty()) {
                QPainterPath path = selectMan->selectionPathWithOffsets();
                if (!selectMan->selectionTransform().isIdentity()) {
                    path = selectMan->selectionTransform().map(path);
                }
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
                    QPainter clearPainter(bitmapImage->image());
                    clearPainter.setRenderHint(QPainter::Antialiasing, false);
                    clearPainter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
                    clearPainter.drawImage(srcTopLeft, mask);

                    bitmapImage->clipHybridStrokesWithMask(mask, bbox.topLeft());
                }
            } else {
                QRectF rect = selectMan->mySelectionRect();
                if (!rect.isEmpty()) {
                    bitmapImage->clear(rect);
                    QImage mask(rect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
                    mask.fill(Qt::white);
                    bitmapImage->clipHybridStrokesWithMask(mask, rect.topLeft());
                }
            }
        }
        mEditor->setModified(mEditor->currentLayerIndex(), mEditor->currentFrame());
        if (mEditor && mEditor->undoRedo()) mEditor->undoRedo()->record(saveState, tr("Delete Selection"));
    }
}

void ScribbleArea::clearImage()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (layer->type() == Layer::VECTOR)
    {
        const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);

        VectorImage* vectorImage = currentVectorImage(layer);
        if (vectorImage != nullptr)
        {
            vectorImage->clear();
        }
        mEditor->select()->clearCurves();
        mEditor->select()->clearVertices();
        if (mEditor && mEditor->undoRedo()) mEditor->undoRedo()->record(saveState, tr("Clear Image"));
    }
    else if (layer->type() == Layer::BITMAP)
    {
        const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);

        BitmapImage* bitmapImage = currentBitmapImage(layer);
        if (bitmapImage == nullptr) return;
        bitmapImage->clear();

        // Invalidate the composite so CanvasPainter recomputes it on next render
        auto bitmapLayer = static_cast<LayerBitmap*>(layer);
        const int cf = mEditor->currentFrame();
        KeyFrame* kf = bitmapLayer->keyExists(cf)
                       ? bitmapLayer->getKeyFrameAt(cf)
                       : bitmapLayer->getLastKeyFrameAtPosition(cf);
        if (kf) {
            BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
            CellLayerStack* stack = bitmapLayer->cellBank()->getCellLayerStack(bkf->cellId());
            if (stack) stack->invalidateComposite();
        }

        if (mEditor && mEditor->undoRedo()) mEditor->undoRedo()->record(saveState, tr("Clear Image"));
    }
    else
    {
        return; // skip updates when nothing changes
    }
    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
}

void ScribbleArea::paletteColorChanged(QColor color)
{
    Q_UNUSED(color)

    for (int i = 0; i < mEditor->layers()->count(); i++)
    {
        Layer* layer = mEditor->layers()->getLayer(i);
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(mEditor->currentFrame());
            if (vectorImage != nullptr)
            {
                vectorImage->modification();
            }
        }
    }

    invalidateAllCache();
}

// Progressive commit methods

void ScribbleArea::commitCompletedTiles()
{
    // Guard checks
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() != Layer::BITMAP) {
        return;
    }
    
    int frameNumber = mEditor->currentFrame();
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    if (bitmapLayer->getLastKeyFrameAtPosition(frameNumber) == nullptr) {
        return;
    }
    
    if (!mTiledBuffer.isValid()) {
        return;
    }
    
    // Identify completed tiles
    QVector<TileIndex> completedTileIndices = mTiledBuffer.identifyCompletedTiles();
    if (completedTileIndices.isEmpty()) {
        return; // Nothing to commit
    }
    
    // Get current bitmap image
    BitmapImage* targetImage = currentBitmapImage(bitmapLayer);
    if (targetImage == nullptr) {
        return;
    }
    
    // Determine composition mode (same logic as paintBitmapBuffer)
    QPainter::CompositionMode compositionMode = QPainter::CompositionMode_SourceOver;
    switch (currentTool()->type()) {
    case ERASER:
        compositionMode = QPainter::CompositionMode_DestinationOut;
        break;
    case PEN:
        if (currentTool()->properties.preserveAlpha) {
            compositionMode = QPainter::CompositionMode_SourceOver;
        }
        break;
    default:
        break;
    }
    
    // If draw-behind is enabled, composite behind existing pixels
    if (currentTool()->properties.drawBehind && compositionMode != QPainter::CompositionMode_DestinationOut) {
        compositionMode = QPainter::CompositionMode_DestinationOver;
    }
    
    // Create temporary buffer with completed tiles
    TiledBuffer tempBuffer;
    QRectF tempBounds;
    
    for (const TileIndex& index : completedTileIndices) {
        Tile* originalTile = mTiledBuffer.getTileFromIndexNoSignal(index);
        if (originalTile) {
            // Sync tile pixmap from image
            originalTile->syncPixmapFromImage();
            
            // Create new tile in temporary buffer
            Tile* newTile = tempBuffer.getTileFromIndexNoSignal(index);
            if (newTile) {
                // Copy pixmap data from original tile to new tile
                newTile->pixmap() = originalTile->pixmap();
                tempBounds = tempBounds.united(newTile->bounds());
            }
        }
    }
    
    // Commit to bitmap image
    targetImage->paste(&tempBuffer, compositionMode);
    bitmapLayer->setModified(frameNumber, true);
    
    // Update canvas display
    QRect screenRect = mEditor->view()->mapCanvasToScreen(tempBounds).toRect();
    update(screenRect);
    
    // Clean up tiled buffer
    mTiledBuffer.markTilesAsCommitted(completedTileIndices);
    mTiledBuffer.removeTiles(completedTileIndices);
}

void ScribbleArea::updateProgressiveCommitDistance(QPointF currentPoint)
{
    // Disable progressive commits during active drawing to prevent broken strokes
    // Progressive commits should only happen between strokes for memory management
    if (currentTool()->isActive() && currentTool()->isDrawingTool()) {
        return;
    }
    
    if (mLastProgressiveCommitPoint.isNull()) {
        mLastProgressiveCommitPoint = currentPoint;
        return;
    }
    
    // Calculate distance from last commit point
    qreal distance = QLineF(mLastProgressiveCommitPoint, currentPoint).length();
    mProgressiveCommitDistance += distance;
    
    // Check if threshold exceeded
    if (mProgressiveCommitDistance >= PROGRESSIVE_COMMIT_THRESHOLD) {
        commitCompletedTiles();
        mProgressiveCommitDistance = 0.0;
        mLastProgressiveCommitPoint = currentPoint;
    }
}

void ScribbleArea::resetProgressiveCommitTracking()
{
    mProgressiveCommitDistance = 0.0;
    mLastProgressiveCommitPoint = QPointF(0, 0);
}

// Navigation optimization implementation
void ScribbleArea::startNavigating()
{
    if (mIsNavigating) {
        if (mNavigationSettleTimer) {
            mNavigationSettleTimer->stop();
            mNavigationSettleTimer->start();
        }
        return;
    }

    mIsNavigating = true;
    mCanvasPainter.setFastMode(true);

    buildFastNavCache();

    if (!mNavigationSettleTimer) {
        mNavigationSettleTimer = new QTimer(this);
        mNavigationSettleTimer->setSingleShot(true);
        mNavigationSettleTimer->setInterval(NAVIGATION_SETTLE_DELAY_MS);
        connect(mNavigationSettleTimer, &QTimer::timeout, this, &ScribbleArea::settleNavigation);
    }
    mNavigationSettleTimer->start();
}

void ScribbleArea::settleNavigation()
{
    if (!mIsNavigating) return;

    mIsNavigating = false;
    mCanvasPainter.setFastMode(false);
    mFastNavCacheValid = false;

    invalidateAllCache();
    update();
}

void ScribbleArea::invalidateFastNavCache()
{
    mFastNavCacheValid = false;
    mLastCachedScale = 0.0;
}

void ScribbleArea::buildFastNavCache()
{
    if (!mEditor || !mEditor->object()) return;

    const Object* object = mEditor->object();
    const int frame = mEditor->currentFrame();
    const int layerCount = object->getLayerCount();

    // Flatten all visible bitmap layers into one canvas-space QImage.
    QRect canvasBounds;
    for (int i = 0; i < layerCount; ++i) {
        Layer* layer = object->getLayer(i);
        if (!layer || !layer->visible()) continue;
        if (layer->type() == Layer::BITMAP) {
            LayerBitmap* bl = static_cast<LayerBitmap*>(layer);
            BitmapImage* img = bl->getLastBitmapImageAtFrame(frame);
            if (!img) continue;
            img->loadFile();
            QImage* qimg = img->image();
            if (!qimg || qimg->isNull()) continue;
            QRect r(img->topLeft(), qimg->size());
            canvasBounds = canvasBounds.isEmpty() ? r : canvasBounds.united(r);
        }
    }

    if (canvasBounds.isEmpty()) {
        mFastNavFlat = QImage();
        mFastNavCacheValid = false;
        return;
    }

    mFastNavFlat = QImage(canvasBounds.size(), QImage::Format_ARGB32_Premultiplied);
    mFastNavFlat.fill(Qt::transparent);

    QPainter flatPainter(&mFastNavFlat);
    flatPainter.setRenderHint(QPainter::Antialiasing, false);
    flatPainter.translate(-canvasBounds.topLeft());

    for (int i = 0; i < layerCount; ++i) {
        Layer* layer = object->getLayer(i);
        if (!layer || !layer->visible()) continue;
        if (layer->type() != Layer::BITMAP) continue;

        LayerBitmap* bl = static_cast<LayerBitmap*>(layer);
        BitmapImage* img = bl->getLastBitmapImageAtFrame(frame);
        if (!img) continue;
        QImage* qimg = img->image();
        if (!qimg || qimg->isNull()) continue;

        flatPainter.setOpacity(layer->opacity());
        flatPainter.drawImage(img->topLeft(), *qimg);
    }
    flatPainter.end();

    mFastNavFlatOrigin = canvasBounds.topLeft();
    mFastNavCacheValid = true;
    mLastCachedScale = mEditor->view()->scaling();
}

void ScribbleArea::onViewChanging()
{
    // Called during active panning/zooming
    startNavigating();
    
    // Mark dirty region for incremental update
    mDirtyRegion += rect();
    
    // Trigger lightweight update
    update();
}

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


#ifndef SCRIBBLEAREA_H
#define SCRIBBLEAREA_H

#include <cstdint>
#include <ctime>
#include <deque>
#include <memory>
#include <functional>

#include <QColor>
#include <QPoint>
#include <QWidget>
#include <QPixmapCache>
#include <QElapsedTimer>

#include "movemode.h"
#include "pencildef.h"
#include "bitmapimage.h"
#include "canvaspainter.h"
#include "overlaypainter.h"
#include "preferencemanager.h"
#include "selectionpainter.h"
#include "camerapainter.h"
#include "tiledbuffer.h"
#include "hybridcanvasstack.h"

class Layer;
class Editor;
class BaseTool;
class PointerEvent;
class BitmapImage;
class VectorImage;
class QTimer;
class QTabletEvent;


class ScribbleArea : public QWidget
{
    Q_OBJECT

    friend class MoveTool;
    friend class SmudgeTool;
    friend class BucketTool;

public:
    ScribbleArea(QWidget* parent);
    ~ScribbleArea() override;

    bool init();
    void setEditor(Editor* e) { mEditor = e; }
    Editor* editor() const { return mEditor; }

    void setPlaybackPresentationModeEnabled(bool enabled);

    void setCanvasInteractionLocked(bool locked);
    bool isCanvasInteractionLocked() const { return mCanvasInteractionLocked; }

    /** Reset mouse/tablet in-use flags. Call after a modal context menu closes
     *  to prevent the canvas from staying unresponsive to navigation/zoom. */
    void resetPointerState();

    void deleteSelection();

    void applyTransformedSelection();
    void cancelTransformedSelection();

    void setEffect(SETTING e, bool isOn);

    LayerVisibility getLayerVisibility() const { return mLayerVisibility; }
    qreal getCurveSmoothing() const { return mCurveSmoothingLevel; }
    bool makeInvisible() const { return mMakeInvisible; }

    QPointF getCentralPoint();

    /** Update frame.
     * calls update() behind the scene and update cache if necessary */
    void updateFrame();

    /** Frame scrubbed, invalidate relevant cache */
    void onScrubbed(int frameNumber);

    /** Multiple frames modified, invalidate cache for affected frames */
    void onFramesModified();

    /** Playstate changed, invalidate relevant cache */
    void onPlayStateChanged();

    /** View updated, invalidate relevant cache */
    void onViewChanged();
    
    /** View is being actively changed (panning/zooming) */
    void onViewChanging();

    /** Frame modified, invalidate cache for frame if any */
    void onFrameModified(int frameNumber);

    /** Layer changed, invalidate relevant cache */
    void onLayerChanged();

    /** Selection was changed, keep cache */
    void onSelectionChanged();

    /** Onion skin type changed, all frames will be affected.
     * All cache will be invalidated */
    void onOnionSkinTypeChanged();

    /** Object updated, invalidate all cache */
    void onObjectLoaded();

    /** Tool property updated, invalidate cache and frame if needed */
    void onToolPropertyUpdated(ToolType, ToolPropertyType);

    /** Tool changed, invalidate cache and frame if needed */
    void onToolChanged(ToolType);

    void endStroke();

    /** Initializes the tiled buffer for batched stroke rendering */
    void beginDrawingStroke(qreal brushWidth = 1.0);
    /** Flushes all accumulated dabs to tiles and emits signals */
    void flushDrawingStroke();
    /** Commits tiles outside active stroke region to bitmap layer */
    void commitCompletedTiles();

    void flipSelection(bool flipVertical);

    BaseTool* currentTool() const;

    bool isMouseInUse() const { return mMouseInUse; }
    bool isTabletInUse() const { return mTabletInUse; }
    bool isPointerInUse() const { return mMouseInUse || mTabletInUse; }

    void keyEvent(QKeyEvent* event);
    void keyEventForSelection(QKeyEvent* event);

signals:
    void multiLayerOnionSkinChanged(bool);
    void selectionUpdated();
    void lockedLayerAttempted(); // emitted when a tool tries to draw on a locked paper

public slots:
    void clearImage();
    void setCurveSmoothing(int);
    void toggleThinLines();
    void increaseLayerVisibilityIndex();
    void decreaseLayerVisibilityIndex();
    void setLayerVisibility(LayerVisibility visibility);

    void updateToolCursor();
    void updateProgressiveCommitDistance(QPointF currentPoint);
    void paletteColorChanged(QColor);

    void showLayerNotVisibleWarning();
    void onTileUpdated(TiledBuffer* tiledBuffer, Tile* tile);
    void onTileCreated(TiledBuffer* tiledBuffer, Tile* tile);

private slots:
    void onShiftTraceMoveStateChanged(int id);

protected:
    bool event(QEvent *event) override;
    void showEvent(QShowEvent* event) override;
    void tabletEvent(QTabletEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

public:
    void drawPolyline(QPainterPath path, QPen pen, bool useAA);
    void drawPath(QPainterPath path, QPen pen, QBrush brush, QPainter::CompositionMode cm);
    void drawPathCanvas(QPainterPath path, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool useAA);
    void drawPen(QPointF thePoint, qreal brushWidth, QColor fillColor, bool useAA = true);
    void drawBrush(QPointF thePoint, qreal brushWidth, qreal offset, QColor fillColor, QPainter::CompositionMode compMode, qreal opacity, bool usingFeather = true, bool useAA = false);
    // Overlay image stamping: draw a pre-rendered dab image into the OpenGL overlay (TiledBuffer)
    // imageBounds must be specified in canvas coordinates
    void drawImageOverlay(const QImage& image, const QRectF& imageBounds, QPainter::CompositionMode cm = QPainter::CompositionMode_SourceOver, bool useAA = true);
    void drawTaperedCurve(const QPainterPath& path, QPen& pen, bool isCommit, qreal taperStartValue, qreal taperEndValue, bool useAA, int taperMode, std::function<void(qreal)> progressCallback = nullptr);
    // Draw a tapered curve for live preview without clearing the existing preview buffer
    void drawTaperedCurveNoClear(const QPainterPath& path, QPen& pen, qreal taperStartPixels, qreal taperEndPixels, bool useAA = false);
    // Erase previously drawn preview stroke along a path with a given width (used for partial redraw)
    void eraseStrokeAlongPath(const QPainterPath& path, qreal width, bool useAA = false);
    void blurBrush(BitmapImage *bmiSource_, QPointF srcPoint_, QPointF thePoint_, qreal brushWidth_, qreal offset_, qreal opacity_);
    void liquifyBrush(BitmapImage *bmiSource_, QPointF srcPoint_, QPointF thePoint_, qreal brushWidth_, qreal offset_, qreal opacity_);
    // Gap-check overlays removed

    void paintBitmapBuffer();
    void clearDrawingBuffer();
    void clearOverlayRect(const QRectF& canvasRect);
    QRectF drawingBufferBounds() const;
    // Progressive commit: commit current tiled buffer segment to the bitmap layer
    void commitCurrentSegmentToLayer();
    void setGaussianGradient(QGradient &gradient, QColor color, qreal opacity, qreal offset);

    // Batched canvas updates in canvas space
    void updateCanvasRegion(const QRectF& canvasBounds);
    // Schedule a deferred canvas update (timer-driven during strokes)
    void scheduleCanvasUpdate();
    // Flush accumulated canvas updates to the screen
    void flushCanvasUpdates();
    // Set stroke active state (affects batching behavior)
    void setStrokeActive(bool active);

    // High DPI: expose current device pixel ratio for UI helpers (e.g., cursor painter)
    qreal devicePixelRatioF() const { return mDevicePixelRatio; }

    void pointerPressEvent(PointerEvent*);
    void pointerMoveEvent(PointerEvent*);
    void pointerReleaseEvent(PointerEvent*);

    /// Call this when starting to use a paint tool. Checks whether we are drawing
    /// on an empty frame, and if so, takes action according to use preference.
    void handleDrawingOnEmptyFrame();

    TiledBuffer mTiledBuffer;
    HybridCanvasStack* hybridStack() { return &mHybridStack; }
    
    // Progress display for long operations (e.g., B-spline commit)
    void showProgress(qreal progress, const QString& label = QString());
    void hideProgress();
    
    // Navigation optimization methods (public for tool access)
    void startNavigating();
    void invalidateFastNavCache();
    
private:

    /** Invalidate the layer pixmap and camera painter caches.
     * Call this in most situations where the layer rendering order is affected.
     * Peviously known as setAllDirty.
    */
    void invalidatePainterCaches();

    /** Invalidate cache for the given frame */
    void invalidateCacheForFrame(int frameNumber);

    /** Invalidate all cache.
     * call this if you're certain that the change you've made affects all frames */
    void invalidateAllCache();

    /** invalidate cache for dirty keyframes. */
    void invalidateCacheForDirtyFrames();

    /** invalidate onion skin cache around frame */
    void invalidateOnionSkinsCacheAround(int frame);

    void prepOverlays(int frame);
    void prepCameraPainter(int frame);
    void prepCanvas(int frame);
    void drawCanvas(int frame, QRect rect);
    void settingUpdated(SETTING setting);
    void paintSelectionVisuals(QPainter &painter);

    void updatePresentationViewTransform();

    BitmapImage* currentBitmapImage(Layer* layer);
    VectorImage* currentVectorImage(Layer* layer) const;

    Editor* mEditor = nullptr;

    LayerVisibility mLayerVisibility = LayerVisibility::ALL;
    bool mMakeInvisible = false;
    qreal mCurveSmoothingLevel = 0.0;
    int mDeltaFactor = 1;

    /* Under certain circumstances a mouse press event will fire after a tablet release event.
       This causes unexpected behaviours for some of the tools, eg. the bucket.
       The problem only seems to occur on windows and only when tapping.
       prior to this fix the event queue would look like this:
       eg: TabletPress -> TabletRelease -> MousePress
       The following will filter mouse events created after a tablet release event.
    */
    void tabletReleaseEventFired();
    bool mMouseInUse = false;
    bool mTabletInUse = false;
    // Set by resetPointerState() when a context menu was shown during a press event.
    // Prevents mousePressEvent from re-setting mMouseInUse=true after we cleared it.
    bool mPointerStateResetPending = false;
    qreal mDevicePixelRatio = 1.;

    // Double click handling for tablet input
    void handleDoubleClick();
    bool mIsFirstClick = true;
    int mDoubleClickMillis = 0;
    // Microsoft suggests that a double click action should be no more than 500 ms
    const int DOUBLE_CLICK_THRESHOLD = 500;
    QTimer* mDoubleClickTimer = nullptr;

    // Tablet navigation throttle: tablets fire 200-500 move events/sec.
    // During navigation (hand tool) we only need ~60fps — cap it here.
    // Press and release events are never throttled.
    QElapsedTimer mTabletNavThrottle;
    static constexpr qint64 TABLET_NAV_THROTTLE_MS = 14; // ~70fps ceiling
    // Tile flush timer for batched tile updates
    QTimer* mTileFlushTimer = nullptr;
    void flushTileUpdates();
    void scheduleTileFlush();
    static const int TILE_FLUSH_DELAY_MS = 16; // ~60fps
    // Canvas update timer and stroke-aware state
    QTimer* mCanvasUpdateTimer = nullptr;
    // Marching-ants animation timer (runs only when a selection exists)
    QTimer* mSelectionAntsTimer = nullptr;
    bool mIsStrokeActive = false;
    static const int CANVAS_UPDATE_DELAY_MS = 16; // ~60fps
    QPointF mTabletPressPos;
    int mTabletReleaseMillisAgo;
    const int MOUSE_FILTER_THRESHOLD = 200;

    QTimer* mMouseFilterTimer = nullptr;

    PreferenceManager* mPrefs = nullptr;

    QPixmap mCanvas;
    CanvasPainter mCanvasPainter;
    OverlayPainter mOverlayPainter;
    SelectionPainter mSelectionPainter;
    CameraPainter mCameraPainter;
    HybridCanvasStack mHybridStack{ this };

    bool mCanvasInteractionLocked = false;

    bool mPlaybackPresentationModeEnabled = false;
    bool mPlaybackPresentationModeActive = false;
    QPointF mPlaybackPresentationSavedTranslation;
    qreal mPlaybackPresentationSavedScaling = 1.0;
    qreal mPlaybackPresentationSavedRotation = 0.0;
    bool mPlaybackPresentationSavedFlipH = false;
    bool mPlaybackPresentationSavedFlipV = false;

    // Accumulated region pending screen update (canvas space)
    QRectF mPendingUpdateRegion;

    // Navigation optimization: fast preview mode
    bool mIsNavigating = false;
    QTimer* mNavigationSettleTimer = nullptr;
    static const int NAVIGATION_SETTLE_DELAY_MS = 120;
    QPixmap mFastNavCache; // kept for compatibility, unused during nav
    bool mFastNavCacheValid = false;
    qreal mLastCachedScale = 1.0;

    // Flattened canvas-space image built once when navigation starts.
    QImage mFastNavFlat;
    QPoint mFastNavFlatOrigin;

    // Scrub repaint throttle — caps canvas repaints to ~60 fps during manual
    // timeline scrubbing so a fast mouse doesn't flood the paint pipeline.
    QElapsedTimer mScrubRepaintThrottle;
    
    // Dirty region tracking for incremental updates
    QRegion mDirtyRegion; // in screen/device coordinates
    
    // Scroll optimization for panning
    QPoint mLastPanPosition; // last pan position in screen coords
    bool mCanScrollBlit = false;

    // Overlays removed

    QPolygonF mOriginalPolygonF = QPolygonF();

    // Pixmap Cache keys
    QMap<unsigned int, QPixmapCache::Key> mPixmapCacheKeys;

    // debug
    QLoggingCategory mLog{ "ScribbleArea" };
    
    // Progress bar for long operations
    bool mShowProgress = false;
    qreal mProgressValue = 0.0;
    QString mProgressLabel;

    // When releasing a temporary-tool key (e.g., Space for Hand) while a pointer is down,
    // we defer clearing the temporary tool until the pointer is released to avoid
    // unintended strokes connecting to previous stroke endpoints.
    bool mIsTempToolClearDeferred = false;
    Qt::Key mDeferredTempToolKey = Qt::Key_unknown;
    
    // Progressive commit tracking
    qreal mProgressiveCommitDistance = 0.0;  // accumulated stroke distance since last commit
    QPointF mLastProgressiveCommitPoint;     // last position where progressive commit occurred
    static constexpr qreal PROGRESSIVE_COMMIT_THRESHOLD = 250.0; // distance threshold for triggering commits
    
    /** Resets distance accumulator and last commit point */
    void resetProgressiveCommitTracking();
    
    // Navigation optimization private methods
    void settleNavigation();
    void buildFastNavCache();
};

#endif

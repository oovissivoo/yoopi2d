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

#include "stroketool.h"

#include <QKeyEvent>
#include <QCursor>
#include <QPen>
#include <QColor>
#include <QPainterPath>
#include "scribblearea.h"
#include "viewmanager.h"
#include "preferencemanager.h"
#include "editor.h"
#include "toolmanager.h"
#include "mathutils.h"
#include "layermanager.h"
#include "layer.h"

#include "canvascursorpainter.h"
#include "pentool.h"

#ifdef Q_OS_MAC
extern "C" {
    void detectWhichOSX();
    void disableCoalescing();
    void enableCoalescing();
}
#else
extern "C" {
    void detectWhichOSX() {}
    void disableCoalescing() {}
    void enableCoalescing() {}
}
#endif

const qreal StrokeTool::FEATHER_MIN = 1.;
const qreal StrokeTool::FEATHER_MAX = 99.;
const qreal StrokeTool::WIDTH_MIN = 1.;
const qreal StrokeTool::WIDTH_MAX = 30.;

// ---- shared static variables ---- ( only one instance for all the tools )
bool StrokeTool::msIsAdjusting = false;
bool StrokeTool::mQuickSizingEnabled = false;

StrokeTool::StrokeTool(QObject* parent) : BaseTool(parent)
{
    detectWhichOSX();
    mLastCursorUpdatePos = QPointF(0, 0);
    mCursorUpdateThreshold = 0.1;
    mThrottleUpdates = false;
    // Note: Throttling is disabled by default to ensure smooth cursor tracking.
    // The icon caching optimization makes per-frame updates very cheap (single blit
    // instead of scaling + outline generation), so distance-based throttling is
    // unnecessary and causes visible "jumpiness" in cursor movement.
    // Throttling can be re-enabled via mThrottleUpdates if needed for low-end hardware.
}

void StrokeTool::loadSettings()
{
    mQuickSizingEnabled = false;
    mCanvasCursorEnabled = mEditor->preference()->isOn(SETTING::CANVAS_CURSOR);

    /// Given the way that we update preferences currently, this connection should not be removed
    /// when the tool is not active.
    connect(mEditor->preference(), &PreferenceManager::optionChanged, this, &StrokeTool::onPreferenceChanged);
}

void StrokeTool::setWidth(const qreal width)
{
    BaseTool::setWidth(width);
    // Clear remembered cursor size when user manually changes width
    mRememberedCursorSize = -1.0;
}

bool StrokeTool::enteringThisTool()
{
    // Call base class to load settings
    BaseTool::enteringThisTool();
    mActiveConnections.append(connect(mEditor->view(), &ViewManager::viewChanged, this, &StrokeTool::onViewUpdated));
    return true;
}

bool StrokeTool::leavingThisTool()
{
    return BaseTool::leavingThisTool();
}

void StrokeTool::onPreferenceChanged(SETTING setting)
{
    if (setting == SETTING::CANVAS_CURSOR) {
    mCanvasCursorEnabled = mEditor->preference()->isOn(setting);
    updateCanvasCursor(true);
    } else if (setting == SETTING::CURSOR_STYLE) {
        updateCanvasCursor(true);
    } else if (setting == SETTING::ALWAYS_SHOW_CIRCLE) {
        updateCanvasCursor(true);
    } else if (setting == SETTING::REMEMBER_CURSOR_SIZE) {
        // Clear remembered size when preference is disabled
        if (!mEditor->preference()->isOn(setting)) {
            mRememberedCursorSize = -1.0;
        }
        updateCanvasCursor(true);
    }
}

void StrokeTool::onViewUpdated()
{
    updateCanvasCursor(true);
}

QPointF StrokeTool::getCurrentPressPixel() const
{
    return mInterpolator.getCurrentPressPixel();
}

QPointF StrokeTool::getCurrentPressPoint() const
{
    return mEditor->view()->mapScreenToCanvas(mInterpolator.getCurrentPressPixel());
}

QPointF StrokeTool::getCurrentPixel() const
{
    return mInterpolator.getCurrentPixel();
}

QPointF StrokeTool::getCurrentPoint() const
{
    return mEditor->view()->mapScreenToCanvas(getCurrentPixel());
}

QPointF StrokeTool::getLastPixel() const
{
    return mInterpolator.getLastPixel();
}

QPointF StrokeTool::getLastPoint() const
{
    return mEditor->view()->mapScreenToCanvas(getLastPixel());
}

void StrokeTool::setLastProcessedPoint(const QPointF& point)
{
    mLastProcessedPoint = point;
}

QPointF StrokeTool::getLastProcessedPoint() const
{
    return mLastProcessedPoint;
}

void StrokeTool::startStroke(PointerEvent::InputType inputType)
{
    if (emptyFrameActionEnabled())
    {
        mScribbleArea->handleDrawingOnEmptyFrame();
    }

    mFirstDraw = true;
    mLastPixel = getCurrentPixel();
    mLastProcessedPoint = getCurrentPoint();

    mStrokePoints.clear();

    //Experimental
    QPointF startStrokes = mInterpolator.interpolateStart(mLastPixel);
    mStrokePoints << mEditor->view()->mapScreenToCanvas(startStrokes);

    mStrokePressures.clear();
    mStrokePressures << mInterpolator.getPressure();

    mCurrentInputType = inputType;
    mUndoSaveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);

    disableCoalescing();

    // Calculate initial brush width for progressive commits
    qreal initialBrushWidth = properties.width;
    if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
        const qreal gw = mEditor->tools()->globalWidth();
        if (gw > 0.0) {
            initialBrushWidth = qMax<qreal>(1.0, gw);
        }
    }

    // Hybrid stack: top preview begin
    if (mScribbleArea) { mScribbleArea->hybridStack()->beginStroke(initialBrushWidth); }
}

bool StrokeTool::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Alt:
        if (mEditor->tools()->beginTemporaryTool(EYEDROPPER, Qt::Key_Alt))
        {
            return true;
        }
        break;
    case Qt::Key_Space:
        if (mEditor->tools()->beginTemporaryTool(HAND, Qt::Key_Space))
        {
            return true;
        }
        break;
    }
    return BaseTool::keyPressEvent(event);
}

bool StrokeTool::emptyFrameActionEnabled()
{
    return true;
}

void StrokeTool::endStroke()
{
    mInterpolator.interpolateEnd();
    mStrokePressures << mInterpolator.getPressure();
    
    // Remember cursor size if preference is enabled and tool supports pressure
    if (mEditor->preference()->isOn(SETTING::REMEMBER_CURSOR_SIZE) && 
        properties.pressure && mEditor->preference()->isOn(SETTING::CURSOR_PRESSURE_SIZE)) {
        const qreal clamped = qBound(0.05, mCurrentPressure, 1.0);
        mRememberedCursorSize = properties.width * clamped;
    }
    
    mStrokePoints.clear();
    mStrokePressures.clear();
    mLastProcessedPoint = QPointF(0, 0);

    enableCoalescing();

    mEditor->setModified(mEditor->currentLayerIndex(), mEditor->currentFrame());
    // Hybrid stack: end lifecycle (persist/hide vector, commit raster handled later)
    if (mScribbleArea) { mScribbleArea->hybridStack()->endStroke(); }
    mScribbleArea->endStroke();

    // Only record if the state hasn't been recorded yet
    // macro logic). Nullify mUndoSaveState after recording to avoid double-
    // recording by callers that may also attempt to record.
    if (mUndoSaveState != nullptr) {
        mEditor->undoRedo()->record(mUndoSaveState, typeName());
        mUndoSaveState = nullptr;
    }
}

void StrokeTool::drawStroke()
{
    QPointF pixel = getCurrentPixel();
    if (pixel != mLastPixel || !mFirstDraw)
    {
        // get last pixel before interpolation initializes
        QPointF startStrokes = mInterpolator.interpolateStart(getLastPixel());
        mStrokePoints << mEditor->view()->mapScreenToCanvas(startStrokes);
        mStrokePressures << mInterpolator.getPressure();
    }
    else
    {
        mFirstDraw = false;
    }

    // Hybrid stack: notify live stroke update (dirty hint TBD)
    if (mScribbleArea) { mScribbleArea->hybridStack()->updateStroke(); }
}

bool StrokeTool::handleQuickSizing(PointerEvent* event)
{
    Q_UNUSED(event)
    return false;

    if (event->eventType() == PointerEvent::Press) {
        if (mQuickSizingEnabled) {
            return startAdjusting(event->modifiers());
        }
    } else if (event->eventType() == PointerEvent::Move) {
        if (event->buttons() & Qt::LeftButton && msIsAdjusting) {
            adjustCursor(event->modifiers());
            return true;
        }
    } else if (event->eventType() == PointerEvent::Release) {
        if (msIsAdjusting) {
            stopAdjusting();
            return true;
        }
    }
    return false;
}

void StrokeTool::pointerPressEvent(PointerEvent*)
{
    mLastCursorUpdatePos = QPointF(0, 0);
    updateCanvasCursor(true);
}

void StrokeTool::pointerMoveEvent(PointerEvent* event)
{
    if (event) {
        mLastPencilCursorEventTimestamp = event->timestamp();
        if (event->isTabletEvent()) {
            const qreal p = qBound<qreal>(0.0, event->pressure(), 1.0);
            const int bucket = qBound(0, int(qFloor(p * 12.0)), 11);
            mLastPencilPressureBucket = bucket;
        }
    }
    updateCanvasCursor();
}

void StrokeTool::pointerReleaseEvent(PointerEvent*)
{
    mLastCursorUpdatePos = QPointF(0, 0);
    updateCanvasCursor(true);
}

bool StrokeTool::enterEvent(QEnterEvent*)
{
    mCanvasCursorEnabled = mEditor->preference()->isOn(SETTING::CANVAS_CURSOR);
    mLastCursorUpdatePos = QPointF(0, 0);
    updateCanvasCursor(true);
    
    return true;
}

bool StrokeTool::leaveEvent(QEvent*)
{
    if (isActive())
    {
        return false;
    }

    mCanvasCursorEnabled = false;
    mLastCursorUpdatePos = QPointF(0, 0);
    updateCanvasCursor(true);
    
    return true;
}

void StrokeTool::updateCanvasCursor(bool force)
{
    if (!mEditor) { return; }
    // In unit tests or headless mode, scribeArea might be null
    if (!mScribbleArea) return;
    
    // Determine current cursor position
    QPointF cursorPos;

    if (msIsAdjusting) {
        cursorPos = mAdjustPosition;
    } else if ((type() == PENCIL || type() == PEN) && mEditor->view()) {
        // CosmicCursorPipeline: drive the canvas cursor from the *current* OS cursor
        // position whenever we are not actively drawing. Using the interpolator's
        // last raw pixel can be stale during view-only interactions (pan/zoom),
        // causing the cursor to "jump" when the next mouse event arrives.
        QPointF rawPixel;
        if (!mInterpolator.isActive()) {
            rawPixel = mScribbleArea->mapFromGlobal(QCursor::pos());
        } else {
            rawPixel = mInterpolator.getLastRawPixel();
        }
        cursorPos = mEditor->view()->mapScreenToCanvas(rawPixel);
    } else {
        cursorPos = getCurrentPoint();
    }

    // Throttle updates unless forced by preference/property updates
    if (!force && !shouldUpdateCursor(cursorPos)) {
        return;
    }

    mLastCursorUpdatePos = cursorPos;

    qreal brushWidth = properties.width;
    
    // Use remembered cursor size if available and not actively drawing
    if (mRememberedCursorSize > 0.0 && !mInterpolator.isActive()) {
        brushWidth = mRememberedCursorSize;
    }
    // Scale cursor by pressure when enabled in preferences and tool supports pressure
    else if (mInterpolator.isActive() && properties.pressure && 
        mEditor->preference()->isOn(SETTING::CURSOR_PRESSURE_SIZE)) {
        const qreal clamped = qBound(0.05, mCurrentPressure, 1.0);
        brushWidth = properties.width * clamped;
    }

    // CosmicCursorPipeline: custom cursor size is fixed and must not depend on the tool brush width.
    // Apply to Pen as well as Pencil.
    if ((type() == PEN || type() == PENCIL) && mEditor->view()) {
        brushWidth = 20.0;
    }
    const qreal brushFeather = properties.feather;

    const qreal cursorRad = brushWidth * 0.5;
    const QPointF& cursorOffset = QPointF(cursorPos.x() - cursorRad, cursorPos.y() - cursorRad);

    CanvasCursorPainterOptions options;
    options.widthRect = QRectF(cursorOffset, QSizeF(brushWidth, brushWidth));

    const qreal featherWidthFactor = MathUtils::normalize(brushFeather, 0.0, FEATHER_MAX);
    options.featherRect = QRectF(options.widthRect.center().x() - (cursorRad * featherWidthFactor),
                                 options.widthRect.center().y() - (cursorRad * featherWidthFactor),
                                 brushWidth * featherWidthFactor,
                                 brushWidth * featherWidthFactor);
    // Show the width cursor in Eraser normal mode only; hide during lasso selection
    bool isEraserLasso = (type() == ERASER) && properties.eraserLassoMode;
    // Honor global canvas cursor preference for all tools; do not force-enable for any tool
    
    // Fix: If we are here (not forced, likely mouse move), ensure mCanvasCursorEnabled is true if the pref is on.
    // This recovers from cases where leaveEvent permanently disabled it but enterEvent was missed.
    if (!force && !mCanvasCursorEnabled && mEditor->preference()->isOn(SETTING::CANVAS_CURSOR)) {
        mCanvasCursorEnabled = true;
    }

    const bool effectiveCursor = mCanvasCursorEnabled;
    // CosmicCursorPipeline: render Pen and Pencil cursors in screen space so size is independent of zoom.
    if (type() == PEN || type() == PENCIL) {
        options.useScreenSpace = true;
        options.screenSpaceSize = 20.0;
    }
    options.showCursor = effectiveCursor && (!isEraserLasso);

    options.isAdjusting = msIsAdjusting && mQuickSizingEnabled;
    options.useFeather = mPropertyEnabled[FEATHER];
    options.cursorStyle = mEditor->preference()->getInt(SETTING::CURSOR_STYLE);

    // Force circle mode for eraser tool
    if (type() == ERASER) {
        options.cursorStyle = 0;
    }

    // Always show circle preference
    options.alwaysShowCircle = mEditor->preference()->isOn(SETTING::ALWAYS_SHOW_CIRCLE);

    // Fixed pixel size for custom cursor icons (base size for cache; actual scaling is done at draw time)
    options.fixedCursorSize = 64.0;
    // High DPI support: pass devicePixelRatio to painter
    if (mScribbleArea) {
        options.devicePixelRatio = mScribbleArea->devicePixelRatioF();
    }

    // Advanced cursor behavior (freeze/hide at speed): apply to PENCIL and PEN (Smooth=0).
    const bool penSmooth0 = (type() == PEN && 
                             ((properties.penSmoothingMode == PenSmoothingMode::PULL) ? (properties.stab2Delay <= 0.0) : (properties.weightedDistance <= 0.0)));

    if ((type() == PENCIL || penSmooth0) && mEditor->view()) {
        const quint64 nowTs = mLastPencilCursorEventTimestamp;
        const quint64 lastTs = mLastPencilCursorUpdateTimestamp;
        const bool hasValidTs = (nowTs > 0 && lastTs > 0);
        const quint64 dt = hasValidTs ? (nowTs - lastTs) : 0;

        if (!force && hasValidTs && dt < quint64(mPencilCursorMinUpdateIntervalMs)) {
            return;
        }

        const qreal v = mInterpolator.getVelocity();
        const bool isDrawing = mInterpolator.isActive() && !msIsAdjusting;
        const bool shouldFreeze = isDrawing && (v >= PENCIL_CURSOR_VELOCITY_THRESHOLD);

        if (shouldFreeze) {
            if (!mPencilCursorFrozen) {
                mPencilFrozenCursorPos = cursorPos;
            }
            mPencilCursorFrozen = true;
            mPencilCursorOpacity = 0.0;
        } else {
            if (mPencilCursorFrozen) {
                if (!mPencilCursorReenableTimer) {
                    mPencilCursorReenableTimer = new QTimer(this);
                    mPencilCursorReenableTimer->setSingleShot(true);
                    connect(mPencilCursorReenableTimer, &QTimer::timeout, this, [this]() {
                        const qreal vv = mInterpolator.getVelocity();
                        const bool drawing = mInterpolator.isActive() && !msIsAdjusting;
                        if (!drawing || vv < PENCIL_CURSOR_VELOCITY_THRESHOLD) {
                            mPencilCursorFrozen = false;
                            mPencilCursorOpacity = 1.0;
                            updateCanvasCursor(true);
                        }
                    });
                }
                if (!mPencilCursorReenableTimer->isActive()) {
                    mPencilCursorReenableTimer->start(60);
                }
            }

            if (!mPencilCursorFrozen) {
                mPencilCursorOpacity = 1.0;
            }
        }

        if (mPencilCursorFrozen) {
            // Clear the cursor once when freezing, then avoid repaint churn while frozen.
            if (!force && mPencilCursorOpacity <= 0.0) {
                options.showCursor = false;
            }
        }

        // Pencil cursor size is fixed for the CosmicCursorPipeline.
        brushWidth = 20.0;

        const qreal cursorRadQ = brushWidth * 0.5;
        const QPointF cursorOffsetQ(cursorPos.x() - cursorRadQ, cursorPos.y() - cursorRadQ);
        options.widthRect = QRectF(cursorOffsetQ, QSizeF(brushWidth, brushWidth));
        const qreal featherWidthFactorQ = MathUtils::normalize(properties.feather, 0.0, FEATHER_MAX);
        options.featherRect = QRectF(options.widthRect.center().x() - (cursorRadQ * featherWidthFactorQ),
                                     options.widthRect.center().y() - (cursorRadQ * featherWidthFactorQ),
                                     brushWidth * featherWidthFactorQ,
                                     brushWidth * featherWidthFactorQ);

        // Base size for cached icons; actual scaling happens at draw time.
        options.fixedCursorSize = (brushWidth <= 32.0) ? 64.0 : 128.0;

        // When opacity is 0 (frozen), treat as hidden so the next repaint clears the previous cursor.
        if (mPencilCursorOpacity <= 0.0) {
            options.showCursor = false;
        }

        mLastPencilCursorUpdateTimestamp = nowTs;
    }

    // Prepare painter state (legacy: cursor is painted in ScribbleArea paint path)
    mCanvasCursorPainter.setDevicePixelRatio(options.devicePixelRatio);
    if (!mEditor->view()) { return; }
    mCanvasCursorPainter.preparePainter(options, mEditor->view()->getView());

    // If the cursor is hidden this frame but we had a previously painted cursor,
    // schedule an update over the last dirty region to erase remnants, then clear state.
    const bool showCursorNow = (options.isAdjusting || options.showCursor);
    if (!showCursorNow && mCanvasCursorPainter.isDirty() && mScribbleArea) {
        mScribbleArea->update(mCanvasCursorPainter.dirtyRect());
        mCanvasCursorPainter.clearDirty();
    }

    const QRect updateRect = mEditor->view()->getView().mapRect(options.widthRect).toAlignedRect();

    // If canvas cursor is disabled and we're not adjusting, we may still need to update
    // the stabilizer circle. Compute its update region first.
    if (!msIsAdjusting && !effectiveCursor) {
        QRect overlayRect;
        if ((type() == PEN || type() == PENCIL) && properties.penSmoothingMode == PenSmoothingMode::PULL) {
            if (mInterpolator.isActive()) {
                const QPointF anchor = mInterpolator.getStab2AnchorPixel();
                const QPointF cursor = mInterpolator.getLastRawPixel();
                const qreal currentDist = QLineF(anchor, cursor).length();
                const qreal leashDistance = properties.stab2Delay;
                
                if (leashDistance > 1.0) {
                    qreal bowDepth = 0.0;
                    if (currentDist < leashDistance * 0.98) {
                        const qreal L2 = leashDistance * leashDistance;
                        const qreal d2 = currentDist * currentDist;
                        bowDepth = qSqrt(qMax(0.0, L2 - d2)) * 0.5;
                    }
                    
                    // Bounding box: line from anchor to cursor + downward bow offset
                    QRectF lineRect = QRectF(anchor, cursor).normalized();
                    lineRect.adjust(-5, -5, 5, bowDepth + 5);
                    overlayRect = lineRect.toAlignedRect();
                    
                    if (!mLastStabCircleRect.isNull()) {
                        overlayRect = overlayRect.united(mLastStabCircleRect);
                    }
                    mLastStabCircleRect = overlayRect;
                } else {
                    mLastStabCircleRect = QRect();
                }
            } else {
                mLastStabCircleRect = QRect();
            }
        } else {
            mLastStabCircleRect = QRect();
        }

        if (!overlayRect.isNull() && mScribbleArea) {
            mScribbleArea->update(overlayRect.adjusted(-2, -2, 2, 2));
        }
        return;
    }

    // Adjusted to account for some pixel bleeding outside the update rect
    QRect u = updateRect.adjusted(-2, -2, 2, 2);
    // Include previous cursor rect to guarantee erasing old location
    if (!mLastCursorRect.isNull()) {
        u = u.united(mLastCursorRect);
    }

    // Also include lazy mouse elastic string overlay area when in Pull mode (Pen tool)
    if ((type() == PEN || type() == PENCIL) && properties.penSmoothingMode == PenSmoothingMode::PULL) {
        if (mInterpolator.isActive()) {
            const QPointF anchor = mInterpolator.getStab2AnchorPixel();
            const QPointF cursor = mInterpolator.getLastRawPixel();
            const qreal currentDist = QLineF(anchor, cursor).length();
            const qreal leashDistance = properties.stab2Delay;
            
            if (leashDistance > 1.0) {
                qreal bowDepth = 0.0;
                if (currentDist < leashDistance * 0.98) {
                    const qreal L2 = leashDistance * leashDistance;
                    const qreal d2 = currentDist * currentDist;
                    bowDepth = qSqrt(qMax(0.0, L2 - d2)) * 0.5;
                }
                
                // Bounding box: line from anchor to cursor + downward bow offset
                QRectF lineRect = QRectF(anchor, cursor).normalized();
                lineRect.adjust(-5, -5, 5, bowDepth + 5);
                QRect stringRect = lineRect.toAlignedRect();
                
                // Union with previous rect to erase old position and avoid trails
                if (!mLastStabCircleRect.isNull()) {
                    u = u.united(mLastStabCircleRect);
                }
                u = u.united(stringRect).adjusted(-2, -2, 2, 2);
                mLastStabCircleRect = stringRect;
            }
        }
    } else {
        mLastStabCircleRect = QRect();
    }

    if (mScribbleArea) {
        mScribbleArea->update(u);
    }
    // Store last cursor rect (widget-space width circle bounds)
    mLastCursorRect = updateRect;
}

bool StrokeTool::shouldUpdateCursor(const QPointF& currentPos) const
{
    if (!mThrottleUpdates) {
        return true;
    }
    if (msIsAdjusting) {
        return true;
    }
    if (mLastCursorUpdatePos.isNull()) {
        return true;
    }
    qreal distance = QLineF(mLastCursorUpdatePos, currentPos).length();
    return distance >= mCursorUpdateThreshold;
}

bool StrokeTool::startAdjusting(Qt::KeyboardModifiers modifiers)
{
    if (!mQuickSizingProperties.contains(modifiers))
    {
        return false;
    }

    const QPointF& currentPressPoint = getCurrentPressPoint();
    const QPointF& currentPoint = getCurrentPoint();
    auto propertyType = mQuickSizingProperties.value(modifiers);
    switch (propertyType) {
    case WIDTH: {
        const qreal factor = 0.5;
        const qreal rad = properties.width * factor;
        const qreal distance = QLineF(currentPressPoint - QPointF(rad, rad), currentPoint).length();
        mAdjustPosition = currentPressPoint - QPointF(distance * factor, distance * factor);
        break;
    }
    case FEATHER: {
        const qreal factor = 0.5;
        const qreal cursorRad = properties.width * factor;
        const qreal featherWidthFactor = MathUtils::normalize(properties.feather, 0.0, FEATHER_MAX);
        const qreal offset = (cursorRad * featherWidthFactor) * factor;
        const qreal distance = QLineF(currentPressPoint - QPointF(offset, offset), currentPoint).length();
        mAdjustPosition = currentPressPoint - QPointF(distance, distance);
        break;
    }
    default:
        Q_UNREACHABLE();
        qWarning() << "Unhandled quick sizing property for tool" << typeName();
        return false;
    }

    msIsAdjusting = true;
    // Lock property edits to this tool while adjusting (prevents misrouting during shortcut switches)
    if (editor() && editor()->tools()) {
        editor()->tools()->beginEditingPropertiesForTool(type());
    }
    updateCanvasCursor(true);
    return true;
}

void StrokeTool::stopAdjusting()
{
    msIsAdjusting = false;
    mAdjustPosition = QPointF();

    if (editor() && editor()->tools()) {
        editor()->tools()->endEditingProperties();
    }
    updateCanvasCursor(true);
}

void StrokeTool::adjustCursor(Qt::KeyboardModifiers modifiers)
{
    switch (mQuickSizingProperties.value(modifiers))
    {
    case WIDTH: {
        // The adjusted position is based on the radius of the circle, so in order to
        // map it back to its original value, we can multiply by the factor we divided with
        const qreal newValue = QLineF(mAdjustPosition, getCurrentPoint()).length() * 2.0;

        mEditor->tools()->setWidth(qBound(WIDTH_MIN, newValue, WIDTH_MAX));
        break;
    }
    case FEATHER: {
        // The radius of the width is the max value we can get
        const qreal inputMin = 0.0;
        const qreal inputMax = properties.width * 0.5;
        const qreal distance = QLineF(mAdjustPosition, getCurrentPoint()).length();
        const qreal outputMax = FEATHER_MAX;
        const qreal outputMin = 0.0;

        // We flip min and max here in order to get the inverted value for the UI
        const qreal mappedValue = MathUtils::map(distance, inputMin, inputMax, outputMax, outputMin);

        mEditor->tools()->setFeather(qBound(FEATHER_MIN, mappedValue, FEATHER_MAX));
        break;
    }
    default:
        Q_UNREACHABLE();
        qWarning() << "Unhandled quick sizing property for tool" << typeName();
    }
    updateCanvasCursor();
}

void StrokeTool::paint(QPainter& painter, const QRect& blitRect)
{
    if (type() == PENCIL) {
        painter.save();
        painter.setOpacity(mPencilCursorOpacity);
        mCanvasCursorPainter.paint(painter, blitRect);
        painter.restore();
    } else {
    mCanvasCursorPainter.paint(painter, blitRect);
    }

    // After painting the cursor, request an update for the union of the previous and current
    // cursor areas so the old cursor gets cleared and no trails are left behind.
    if (mCanvasCursorPainter.isDirty() && mScribbleArea) {
        mScribbleArea->update(mCanvasCursorPainter.dirtyRect());
        // Clear dirty so we only schedule once per frame
        mCanvasCursorPainter.clearDirty();
    }

    // Draw Lazy Mouse elastic string/leash visual when using Pull mode
    if (type() == PEN || type() == PENCIL) {
        const Properties& p = properties;
        if (p.penSmoothingMode == PenSmoothingMode::PULL && mInterpolator.isActive()) {
            painter.save();
            painter.setClipRect(blitRect);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setRenderHint(QPainter::Antialiasing, true);

            const qreal leashDistance = p.stab2Delay;
            const QPointF anchor = mInterpolator.getStab2AnchorPixel();
            const QPointF cursor = mInterpolator.getLastRawPixel();
            const qreal currentDist = QLineF(anchor, cursor).length();
            
            // Only draw string if there's leash distance set
            if (leashDistance > 1.0) {
                // String arc length should always equal leashDistance
                // When currentDist < leashDistance, we have slack → string sags
                // When currentDist >= leashDistance, string is taut → straight line
                
                QPainterPath stringPath;
                stringPath.moveTo(anchor);
                
                if (currentDist >= leashDistance * 0.98) {
                    // At or beyond leash limit: string is taut (straight line)
                    stringPath.lineTo(cursor);
                } else {
                    // String has slack: calculate bow depth for constant arc length
                    // For a quadratic bezier maintaining arc length L with endpoints distance d apart:
                    // h = sqrt(L² - d²) / 2
                    // This ensures: closer to center = deeper bow, at limit = straight line
                    
                    const qreal L2 = leashDistance * leashDistance;
                    const qreal d2 = currentDist * currentDist;
                    const qreal bowDepth = qSqrt(qMax(0.0, L2 - d2)) * 0.5;
                    
                    // Calculate bow point with gravity-driven sag
                    // Rope hangs downward due to gravity, not perpendicular to the line
                    QPointF midPoint = (anchor + cursor) / 2.0;
                    
                    // Bow point: geometric midpoint offset downward (+Y) by bow depth
                    QPointF bowPoint = midPoint + QPointF(0, bowDepth);
                    
                    // Quadratic bezier curve through bow point
                    stringPath.quadTo(bowPoint, cursor);
                }
                
                // Draw black outline (2px)
                QPen outlinePen(Qt::black);
                outlinePen.setWidthF(2.0);
                outlinePen.setCosmetic(true);
                outlinePen.setCapStyle(Qt::RoundCap);
                painter.setPen(outlinePen);
                painter.drawPath(stringPath);
                
                // Draw white center line (1px) on top
                QPen centerPen(Qt::white);
                centerPen.setWidthF(1.0);
                centerPen.setCosmetic(true);
                centerPen.setCapStyle(Qt::RoundCap);
                painter.setPen(centerPen);
                painter.drawPath(stringPath);
            }
            
            painter.restore();
        }
    }
}

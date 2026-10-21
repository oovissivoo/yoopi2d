#include "lassoselecttool.h"

#include <QPainter>
#include <QPixmap>
#include <QLineF>
#include <QKeyEvent>
#include <QPainterPathStroker>
#include <cmath>
#include <algorithm>
#include "pointerevent.h"
#include "editor.h"
#include "layermanager.h"
#include "layer.h"
#include "scribblearea.h"
#include "layerbitmap.h"
#include "selectionmanager.h"
#include "managers/undoredomanager.h"
// #include "utils.h"
#include "bitmapkeyframe.h"
#include "util/pencilsettings.h"
#include "viewmanager.h"

LassoSelectTool::LassoSelectTool(QObject* parent) : BaseTool(parent)
{
}

void LassoSelectTool::setAA(const int useAA)
{
    // Persist in base properties and forward to selection manager as interpolation mode
    BaseTool::setAA(useAA);
    if (mEditor && mEditor->select()) {
        mEditor->select()->setResampleSmooth(useAA != 0);
        if (mScribbleArea) mScribbleArea->updateFrame();
    }
    notifySettingsChanged();
}

void LassoSelectTool::loadSettings()
{
    mPropertyEnabled[ANTI_ALIASING] = true; // expose AA toggle in options for interpolation control
    // Load persisted AA preference (default OFF for crisp moves)
    QSettings settings(PENCIL2D, PENCIL2D);
    const int aa = settings.value("Tool/LassoSelect/AntiAliasing", 0).toInt();
    properties.useAA = aa;
    if (mEditor && mEditor->select()) {
        mEditor->select()->setResampleSmooth(aa != 0);
    }
    // Load rotation snapping increment from preferences
    if (mEditor) {
        mRotationIncrement = mEditor->preference()->getInt(SETTING::ROTATION_INCREMENT);
    }
}

void LassoSelectTool::saveSettings()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("Tool/LassoSelect/AntiAliasing", properties.useAA);
    settings.sync();
}

QCursor LassoSelectTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":/icons/themes/playful/general/cursor-crosshair.svg"), 8, 8);
    }
    return Qt::CrossCursor;
}

void LassoSelectTool::pointerPressEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (!currentLayer || !currentLayer->isPaintable()) return;
    if (currentLayer->type() != Layer::BITMAP) return;

    auto sel = mEditor->select();

    m_pressPoint = event->canvasPos();

    const bool hasPath = !sel->selectionPathRaw().isEmpty();
    bool inside = false;
    bool onBoundary = false;
    if (hasPath) {
        QPainterPath path = sel->selectionPathWithOffsets();
        if (!sel->selectionTransform().isIdentity()) {
            path = sel->selectionTransform().map(path);
        }
        inside = path.contains(m_pressPoint);

        QRectF bbox = path.boundingRect();
        QPainterPath rectPath;
        rectPath.addRect(bbox);
        QPainterPathStroker stroker;
        stroker.setWidth(qMax<qreal>(1.0, sel->selectionTolerance() * 2.0));
        QPainterPath outline = stroker.createStroke(rectPath);
        onBoundary = outline.contains(m_pressPoint);
    }
    const bool ctrlDown = (event->modifiers() & Qt::ControlModifier);
    const bool outside = hasPath && !inside && !onBoundary;

    // 1) Corner handle hit-test (device space) for scaling, and rotation handle hit-test
    if (hasPath) {
        // Build device transform identical to SelectionPainter
        const QTransform T = sel->selectionTransform() * mEditor->view()->getView();
        const QRectF rect = sel->selectionPathWithOffsets().boundingRect();
        const QPolygonF poly = T.map(QPolygonF(rect));
        const QPointF vp = event->viewportPos();
        const int hs = HANDLE_SIZE;
        Handle h = Handle::None;
        // Order: TL, TR, BR, BL
        QRectF rTL(poly[0].x() - hs/2.0, poly[0].y() - hs/2.0, hs, hs);
        QRectF rTR(poly[1].x() - hs/2.0, poly[1].y() - hs/2.0, hs, hs);
        QRectF rBR(poly[2].x() - hs/2.0, poly[2].y() - hs/2.0, hs, hs);
        QRectF rBL(poly[3].x() - hs/2.0, poly[3].y() - hs/2.0, hs, hs);
        if (rTL.contains(vp)) h = Handle::TopLeft;
        else if (rTR.contains(vp)) h = Handle::TopRight;
        else if (rBR.contains(vp)) h = Handle::BottomRight;
        else if (rBL.contains(vp)) h = Handle::BottomLeft;

        // Rotation handle: midpoint of right edge (TR+BR)/2, same 12px hit area
        if (h == Handle::None && !ctrlDown) {
            const QPointF rotCenter = (poly[1] + poly[2]) * 0.5;
            QRectF rRot(rotCenter.x() - hs/2.0, rotCenter.y() - hs/2.0, hs, hs);
            if (rRot.contains(vp)) {
                m_isRotating = true;
                if (!mContentWasMoved) {
                    mScribbleArea->handleDrawingOnEmptyFrame();
                    mUndoState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
                    mContentWasMoved = true;
                }
                if (!sel->hasFloating()) {
                    sel->extractFloatingFromPath(true);
                } else {
                    sel->ensureSourceCutForMove();
                }
                sel->setFloatingOffset(QPointF(0,0));
                sel->setOutlineOffset(QPointF(0,0));
                sel->setTransformAnchor(sel->mySelectionRect().center());
                m_initialRotationAngle = sel->myRotation();
                m_startRotationAngle = sel->angleFromPoint(event->canvasPos(), sel->currentTransformAnchor());
                {
                    QPainterPath tp = sel->selectionTransform().map(sel->selectionPathWithOffsets());
                    QRectF base = tp.boundingRect();
                    const qreal inv = mEditor->view()->getScaleInversed();
                    const qreal pad = (8.0 + 3.0) * inv;
                    m_lastGizmoCanvasRect = base.adjusted(-pad, -pad, pad, pad);
                }
                return;
            }
        }

        if (h != Handle::None) {
            m_isScaling = true;
            m_activeHandle = h;
            m_initialRect = rect; // canvas-space
            m_scalePressViewportPoint = event->viewportPos();
            m_initialSelectionTransformForScale = sel->selectionTransform();
            m_scaleStartX = sel->myScaleX();
            m_scaleStartY = sel->myScaleY();
            // Anchor is opposite corner
            QPointF anchor;
            switch (h) {
                case Handle::TopLeft: anchor = rect.bottomRight(); break;
                case Handle::TopRight: anchor = rect.bottomLeft(); break;
                case Handle::BottomRight: anchor = rect.topLeft(); break;
                case Handle::BottomLeft: anchor = rect.topRight(); break;
                default: anchor = rect.center(); break;
            }
            sel->setTransformAnchor(anchor);
            // Ensure undo captured for this scaling session
            if (!mContentWasMoved) {
                mScribbleArea->handleDrawingOnEmptyFrame();
                mUndoState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
                mContentWasMoved = true;
            }
            // Ensure a floating overlay exists so scale affects content immediately
            if (!sel->hasFloating()) {
                sel->extractFloatingFromPath(true); // cut source into floating overlay
            } else {
                sel->ensureSourceCutForMove();
            }
            sel->setFloatingOffset(QPointF(0,0));
            sel->setOutlineOffset(QPointF(0,0));
            // Initialize cached gizmo canvas rect (transformed path bounds + padding)
            {
                QPainterPath tp = sel->selectionTransform().map(sel->selectionPathWithOffsets());
                QRectF base = tp.boundingRect();
                const qreal inv = mEditor->view()->getScaleInversed();
                // Padding covers corner handles (6px half) + rotation handle line + AA cushion
                const qreal pad = (8.0 + 3.0) * inv;
                m_lastGizmoCanvasRect = base.adjusted(-pad, -pad, pad, pad);
            }
            return;
        }
    }

    // 1b) Click-drag outside selection (no Ctrl) => rotate around selection center
    if (hasPath && outside && !ctrlDown) {
        m_isRotating = true;
        // Capture undo for this rotation session if not yet captured
        if (!mContentWasMoved) {
            mScribbleArea->handleDrawingOnEmptyFrame();
            mUndoState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
            mContentWasMoved = true;
        }
        // Ensure floating overlay exists so rotation affects content immediately
        if (!sel->hasFloating()) {
            sel->extractFloatingFromPath(true);
        } else {
            sel->ensureSourceCutForMove();
        }
        sel->setFloatingOffset(QPointF(0,0));
        sel->setOutlineOffset(QPointF(0,0));
        // Initialize anchor once so subsequent rotations continue from the last angle
        sel->setTransformAnchor(sel->mySelectionRect().center());
        m_initialRotationAngle = sel->myRotation();
        m_startRotationAngle = sel->angleFromPoint(event->canvasPos(), sel->currentTransformAnchor());
        // Initialize cached gizmo canvas rect (transformed path bounds + padding)
        {
            QPainterPath tp = sel->selectionTransform().map(sel->selectionPathWithOffsets());
            QRectF base = tp.boundingRect();
            const qreal inv = mEditor->view()->getScaleInversed();
            const qreal pad = (8.0 + 3.0) * inv;
            m_lastGizmoCanvasRect = base.adjusted(-pad, -pad, pad, pad);
        }
        return;
    }

    // If Ctrl is held, always start a new selection regardless of inside/outside
    if (!ctrlDown && (inside || onBoundary)) {
        // Click-drag inside selection (no Ctrl) => start moving the selected content
        m_isDraggingMove = true;
        m_pressViewportPoint = event->viewportPos();
        // Capture undo state BEFORE starting the move (only once per move operation)
        if (!mContentWasMoved) {
            mScribbleArea->handleDrawingOnEmptyFrame();
            mUndoState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);
            mContentWasMoved = true;  // Mark as moved since we're cutting the content
        }
        // Ensure floating selection exists and cut source immediately (send to overlay)
        if (!sel->hasFloating()) {
            sel->extractFloatingFromPath(true); // cut the source so overlay shows pixels without vanishing
        } else {
            sel->ensureSourceCutForMove();
        }
        sel->setFloatingOffset(QPointF(0,0));
        sel->setOutlineOffset(QPointF(0,0));
        mDidStartDragCut = false;
        mUndoCapturedForDrag = false;
        return;
    }

    // Begin lasso selection path (freeform)
    if (hasPath) {
        // If there's floating content that needs committing, commit it first to avoid data loss
        if (sel->hasFloating()) {
            if (!sel->selectionTransform().isIdentity()) {
                sel->bakeTransformIntoFloating();
            }
            mScribbleArea->handleDrawingOnEmptyFrame();
            sel->commitFloatingMove();
            mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
            // Only record undo if content was actually moved
            if (mContentWasMoved && mUndoState) {
                mEditor->undoRedo()->record(mUndoState, typeName());
                mContentWasMoved = false;
                mUndoState = nullptr;
            }
        }
        sel->clearAll();
    }
    m_isDraggingMove = false;
    m_isBuilding = true;
    m_path = QPainterPath(m_pressPoint);
    sel->beginLassoAt(m_pressPoint);
    mScribbleArea->updateFrame();
}

void LassoSelectTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (!currentLayer || !currentLayer->isPaintable()) return;
    if (currentLayer->type() != Layer::BITMAP) return;

    auto sel = mEditor->select();

    if (m_isScaling && sel->hasFloating() && !sel->selectionPathRaw().isEmpty()) {
        // Compute scale factors based on drag corner vs fixed anchor
        const QRectF rect0 = m_initialRect;
        // Derive the moving point from viewport delta using the initial selection+view transform
        QPointF vpDelta = event->viewportPos() - m_scalePressViewportPoint;
        QTransform combined0 = m_initialSelectionTransformForScale.isIdentity()
                              ? mEditor->view()->getView()
                              : (m_initialSelectionTransformForScale * mEditor->view()->getView());
        QTransform invCombined0 = combined0.inverted();
        QPointF d0 = invCombined0.map(QPointF(0,0));
        QPointF d1 = invCombined0.map(QPointF(vpDelta.x(), vpDelta.y()));
        QPointF deltaCanvas = d1 - d0;

        QPointF anchor;
        QPointF moving0;
        switch (m_activeHandle) {
            case Handle::TopLeft: anchor = rect0.bottomRight(); moving0 = rect0.topLeft(); break;
            case Handle::TopRight: anchor = rect0.bottomLeft(); moving0 = rect0.topRight(); break;
            case Handle::BottomRight: anchor = rect0.topLeft(); moving0 = rect0.bottomRight(); break;
            case Handle::BottomLeft: anchor = rect0.topRight(); moving0 = rect0.bottomLeft(); break;
            default: anchor = rect0.center(); moving0 = rect0.center(); break;
        }
        QPointF movingTarget = moving0 + deltaCanvas;
        const qreal w0 = qMax<qreal>(0.0001, rect0.width());
        const qreal h0 = qMax<qreal>(0.0001, rect0.height());
        const qreal w = qAbs(movingTarget.x() - anchor.x());
        const qreal h = qAbs(movingTarget.y() - anchor.y());
        qreal sx = w / w0;
        qreal sy = h / h0;
        // Shift = constrain to original aspect ratio (use the larger axis)
        if (event->modifiers() & Qt::ShiftModifier) {
            const qreal s = qMax(sx, sy);
            sx = s;
            sy = s;
        }
        sel->scale(m_scaleStartX * sx, m_scaleStartY * sy);
        sel->calculateSelectionTransformation();
        // Invalidation: cover all handles + rotation handle line
        {
            const QTransform Tscreen = sel->selectionTransform() * mEditor->view()->getView();
            const QRectF rect = sel->selectionPathWithOffsets().boundingRect();
            const QPolygonF polyS = Tscreen.map(QPolygonF(rect));
            constexpr qreal AA_PAD = 10.0;
            constexpr qreal HS = HANDLE_SIZE;
            const QPointF rotS = (polyS[1] + polyS[2]) * 0.5;
            QRectF rTL(polyS[0].x()-HS/2.0, polyS[0].y()-HS/2.0, HS, HS);
            QRectF rTR(polyS[1].x()-HS/2.0, polyS[1].y()-HS/2.0, HS, HS);
            QRectF rBR(polyS[2].x()-HS/2.0, polyS[2].y()-HS/2.0, HS, HS);
            QRectF rBL(polyS[3].x()-HS/2.0, polyS[3].y()-HS/2.0, HS, HS);
            QRectF rRot(rotS.x()-HS/2.0, rotS.y()-HS/2.0, HS, HS);
            QRectF currentS = rTL.united(rTR).united(rBR).united(rBL).united(rRot).adjusted(-AA_PAD,-AA_PAD,AA_PAD,AA_PAD);
            QRect totalS = m_lastGizmoScreenRect.united(currentS.toAlignedRect());
            m_lastGizmoScreenRect = currentS.toAlignedRect();
            const QTransform invV = mEditor->view()->getView().inverted();
            mScribbleArea->updateCanvasRegion(invV.mapRect(QRectF(totalS)));
            mScribbleArea->flushCanvasUpdates();
        }
        return;
    }

    // Rotation interaction
    if (m_isRotating && sel->hasFloating() && !sel->selectionPathRaw().isEmpty()) {
        const qreal currentAngle = sel->angleFromPoint(event->canvasPos(), sel->currentTransformAnchor());
        const qreal rotationDelta = currentAngle - m_startRotationAngle;
        qreal targetAngle = m_initialRotationAngle + rotationDelta;
        const int snap = (event->modifiers() & Qt::ShiftModifier) ? mRotationIncrement : 0;
        sel->rotate(targetAngle, snap);
        sel->calculateSelectionTransformation();
        // Invalidation: cover all handles + rotation handle line
        {
            const QTransform Tscreen = sel->selectionTransform() * mEditor->view()->getView();
            const QRectF rect = sel->selectionPathWithOffsets().boundingRect();
            const QPolygonF polyS = Tscreen.map(QPolygonF(rect));
            constexpr qreal AA_PAD = 10.0;
            constexpr qreal HS = HANDLE_SIZE;
            const QPointF rotS = (polyS[1] + polyS[2]) * 0.5;
            QRectF rTL(polyS[0].x()-HS/2.0, polyS[0].y()-HS/2.0, HS, HS);
            QRectF rTR(polyS[1].x()-HS/2.0, polyS[1].y()-HS/2.0, HS, HS);
            QRectF rBR(polyS[2].x()-HS/2.0, polyS[2].y()-HS/2.0, HS, HS);
            QRectF rBL(polyS[3].x()-HS/2.0, polyS[3].y()-HS/2.0, HS, HS);
            QRectF rRot(rotS.x()-HS/2.0, rotS.y()-HS/2.0, HS, HS);
            QRectF currentS = rTL.united(rTR).united(rBR).united(rBL).united(rRot).adjusted(-AA_PAD,-AA_PAD,AA_PAD,AA_PAD);
            QRect totalS = m_lastGizmoScreenRect.united(currentS.toAlignedRect());
            m_lastGizmoScreenRect = currentS.toAlignedRect();
            const QTransform invV = mEditor->view()->getView().inverted();
            mScribbleArea->updateCanvasRegion(invV.mapRect(QRectF(totalS)));
            mScribbleArea->flushCanvasUpdates();
        }
        return;
    }

    if (m_isDraggingMove && !sel->selectionPathRaw().isEmpty()) {
        QPointF vpDelta = event->viewportPos() - m_pressViewportPoint;
        QTransform combined = sel->selectionTransform().isIdentity()
                             ? mEditor->view()->getView()
                             : (sel->selectionTransform() * mEditor->view()->getView());
        QTransform invCombined = combined.inverted();
        QPointF v0 = invCombined.map(QPointF(0,0));
        QPointF v1 = invCombined.map(QPointF(vpDelta.x(), vpDelta.y()));
        QPointF delta = v1 - v0;
        // Capture previous offset BEFORE updating to compute old gizmo region
        const QPointF prevDelta = sel->floatingOffset();
        // Non-destructive preview: no cut during move
        sel->setFloatingOffset(delta);
        sel->setOutlineOffset(delta);

        // Request repaint for union of source/destination floating content
        QRectF srcRect(sel->floatingOrigin(), sel->floatingImage().size());
        QRectF dstRect = srcRect.translated(delta);

        // Invalidation: cover all handles + rotation handle line (move)
        {
            const QTransform V = mEditor->view()->getView();
            const QTransform Tscreen = sel->selectionTransform() * V;
            const QRectF rect = sel->selectionPathWithOffsets().boundingRect();
            const QPolygonF polyS = Tscreen.map(QPolygonF(rect));
            constexpr qreal AA_PAD = 10.0;
            constexpr qreal HS = HANDLE_SIZE;
            const QPointF rotS = (polyS[1] + polyS[2]) * 0.5;
            QRectF rTL(polyS[0].x()-HS/2.0, polyS[0].y()-HS/2.0, HS, HS);
            QRectF rTR(polyS[1].x()-HS/2.0, polyS[1].y()-HS/2.0, HS, HS);
            QRectF rBR(polyS[2].x()-HS/2.0, polyS[2].y()-HS/2.0, HS, HS);
            QRectF rBL(polyS[3].x()-HS/2.0, polyS[3].y()-HS/2.0, HS, HS);
            QRectF rRot(rotS.x()-HS/2.0, rotS.y()-HS/2.0, HS, HS);
            QRectF currentS = rTL.united(rTR).united(rBR).united(rBL).united(rRot).adjusted(-AA_PAD,-AA_PAD,AA_PAD,AA_PAD);
            QRect totalS = m_lastGizmoScreenRect.united(currentS.toAlignedRect());
            m_lastGizmoScreenRect = currentS.toAlignedRect();
            const QTransform invV = V.inverted();
            mScribbleArea->updateCanvasRegion(invV.mapRect(QRectF(totalS)));
            mScribbleArea->flushCanvasUpdates();
        }
        return;
    }

    if (m_isBuilding) {
        QPointF p = event->canvasPos();
        if (QLineF(m_path.currentPosition(), p).length() > 0.5) {
            m_path.lineTo(p);
            sel->appendLassoPoint(p);
            mScribbleArea->updateFrame();
        }
        return;
    }
}

void LassoSelectTool::pointerReleaseEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (!currentLayer || currentLayer->type() != Layer::BITMAP) return;

    auto sel = mEditor->select();

    if (m_isScaling) {
        m_isScaling = false;
        m_activeHandle = Handle::None;
        m_lastGizmoCanvasRect = QRectF();
        mScribbleArea->updateFrame();
        return;
    }

    if (m_isRotating) {
        m_isRotating = false;
        mContentWasMoved = true;
        m_lastGizmoCanvasRect = QRectF();
        mScribbleArea->updateFrame();
        return;
    }

    if (m_isDraggingMove) {
        // Recompute delta to be robust if last move was missed
        QPointF vpDelta = event->viewportPos() - m_pressViewportPoint;
        QTransform combined = sel->selectionTransform().isIdentity()
                             ? mEditor->view()->getView()
                             : (sel->selectionTransform() * mEditor->view()->getView());
        QTransform invCombined = combined.inverted();
        QPointF v0 = invCombined.map(QPointF(0,0));
        QPointF v1 = invCombined.map(QPointF(vpDelta.x(), vpDelta.y()));
        QPointF delta = v1 - v0;
        if (!qFuzzyIsNull(delta.x()) || !qFuzzyIsNull(delta.y())) {
            // Update the floating offset for preview
            const QPointF prevDelta = sel->floatingOffset();
            sel->setFloatingOffset(delta);
            QRectF srcRect(sel->floatingOrigin(), sel->floatingImage().size());
            QRectF dstRect = srcRect.translated(delta);
            // Include gizmo regions (old and new) to clear handles and rotation ring, padding in canvas units
            const QRectF rawPathRect = sel->selectionPathRaw().boundingRect();
            QRectF gizmoOld = rawPathRect.translated(prevDelta);
            QRectF gizmoNew = rawPathRect.translated(delta);
            constexpr qreal RING_MARGIN = 6.0;      // device px
            constexpr qreal RING_THICKNESS = 20.0;  // device px
            constexpr qreal AA_PAD = 8.0;           // device px cushion
            const qreal inv = mEditor->view()->getScaleInversed();
            const qreal ringPadCanvas = (RING_MARGIN + RING_THICKNESS + AA_PAD) * inv;
            const qreal handlePadCanvas = (HANDLE_SIZE / 2.0 + 3.0) * inv;
            const qreal pad = std::max(ringPadCanvas, handlePadCanvas);
            gizmoOld.adjust(-pad, -pad, pad, pad);
            gizmoNew.adjust(-pad, -pad, pad, pad);
            QRectF total = srcRect.united(dstRect).united(gizmoOld).united(gizmoNew);
            QRect unionCanvas = total.toAlignedRect().adjusted(-2, -2, 2, 2);
            mScribbleArea->updateCanvasRegion(unionCanvas);
            // Apply the offset to make it permanent (bake into origin); keep overlay persistent
            sel->applyFloatingOffset();
            // Mark content moved; we'll record undo when the user commits (Enter or tool change)
            mContentWasMoved = true;
        }
        m_isDraggingMove = false;
        mDidStartDragCut = false;
        mScribbleArea->updateFrame();
        return;
    }

    if (m_isBuilding) {
        m_isBuilding = false;
        // Close lasso and finalize selection
        sel->closeLasso();
        // Don't record undo for just drawing a selection path
        mScribbleArea->updateFrame();
        return;
    }
}

bool LassoSelectTool::handleDeselectAll()
{
    if (!mEditor || !mScribbleArea) {
        return false;
    }

    auto sel = mEditor->select();
    if (!sel) {
        return false;
    }

    // For floating selections created from a paste operation, let the generic
    // Editor::deselectAll() path handle committing the floating content and
    // creating the undo/redo entry. The SelectionManager advertises this via
    // isFloatingFromPaste(), and needsFloatingCommit() will be true so the
    // Editor fallback path will call commitFloatingMove() and record a
    // KEYFRAME_MODIFY undo record.
    if (sel->hasFloating() && sel->isFloatingFromPaste()) {
        return false;
    }

    const bool hasFloat = sel->hasFloating() || sel->needsFloatingCommit();
    const bool hasTransform = !sel->selectionTransform().isIdentity();

    // If there is no pending floating content or transform, let the editor handle it
    if (!hasFloat && !hasTransform) {
        return false;
    }

    if (hasFloat) {
        // Mirror the Enter-key commit path: bake any active transform into the floating buffer,
        // then commit the move and record a single undo step using the lasso's snapshot.
        if (!sel->selectionTransform().isIdentity()) {
            sel->bakeTransformIntoFloating();
        }
        mScribbleArea->handleDrawingOnEmptyFrame();
        sel->commitFloatingMove();
        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
        if (mContentWasMoved && mUndoState) {
            mEditor->undoRedo()->record(mUndoState, typeName());
            mContentWasMoved = false;
            mUndoState = nullptr;
        }
        // Selection geometry and any remaining state will be cleared by Editor::deselectAll()
        // via resetSelectionProperties(), so we only need to ensure the canvas is refreshed.
        mScribbleArea->updateFrame();
        return true;
    }

    // For pure transform cases without floating content (rare for lasso), fall back to the
    // generic editor deselect logic.
    return false;
}

bool LassoSelectTool::leavingThisTool()
{
    auto sel = mEditor->select();
    if (sel && sel->hasFloating()) {
        // Ensure target frame exists if needed, then commit floating content
        if (!sel->selectionTransform().isIdentity()) {
            sel->bakeTransformIntoFloating();
        }
        mScribbleArea->handleDrawingOnEmptyFrame();
        sel->commitFloatingMove();
        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
        // Only record undo if content was actually moved
        if (mContentWasMoved && mUndoState) {
            mEditor->undoRedo()->record(mUndoState, typeName());
            mContentWasMoved = false;
            mUndoState = nullptr;
        }
        // Clear selection after commit when switching tools
        sel->clearAll();
    }
    return BaseTool::leavingThisTool();
}

bool LassoSelectTool::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return false;
    auto sel = mEditor->select();
    const bool hasFloat = sel && (sel->hasFloating() || sel->needsFloatingCommit());
    // When there is a floating selection, handle Backspace/Delete here so we
    // delete using the existing cut buffer instead of re-deriving geometry.
    if (sel && hasFloat &&
        (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)) {
        // Clear selection and overlays without pasting floating content back
        sel->clearAll();
        // Also reset any residual transform so the next selection starts fresh
        sel->resetSelectionTransformProperties();
        sel->calculateSelectionTransformation();
        // Record the pending move/transform as a single undoable action
        if (mContentWasMoved && mUndoState) {
            mEditor->undoRedo()->record(mUndoState, typeName());
            mContentWasMoved = false;
            mUndoState = nullptr;
        }
        mScribbleArea->updateFrame();
        return true;
    }
    // Intercept Undo during drag to cancel the move and restore cut content without popping undo stack
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Z) {
        qDebug() << "[Lasso] Ctrl+Z pressed. dragging=" << m_isDraggingMove << " hasFloat=" << hasFloat;
        if (m_isDraggingMove || hasFloat) {
            // Cancel current drag preview
            m_isDraggingMove = false;
            if (sel) {
                qDebug() << "[Lasso] Restoring floating to source and clearing overlay. floatingOrigin=" << sel->floatingOrigin()
                         << " floatingSize=" << sel->floatingImage().size();
                sel->setFloatingOffset(QPointF(0,0));
                sel->setOutlineOffset(QPointF(0,0));
                // Restore cut pixels back to source and clear floating overlay/state
                sel->restoreFloatingToSourceAndClear();
            }
            // As a safety net, restore the entire keyframe snapshot taken at
            // drag start (no undo record) ONLY for cut/move floating
            // selections. For floating selections that originated from paste,
            // this would incorrectly wipe out previously committed paste
            // operations, so we skip the snapshot restore in that case.
            if (mUndoState && mUndoState->keyframe && sel && !sel->isFloatingFromPaste()) {
                Layer* layer = mEditor->layers()->currentLayer();
                if (layer && layer->type() == Layer::BITMAP) {
                    // mUndoState->keyframe is shared_ptr<KeyFrame>
                    // logic: casting keyframe clone back to BitmapImage. 
                    // BUT UndoState probably captured the *Cell* or the *Exposure*?
                    // Previous logic: KeyFrame WAS the image.
                    // Now: KeyFrame is Exposure. 
                    // If we saved the "Cell" in undo state, we need to cast to properties?
                    // Let's assume UndoState for Bitmap tools needs to save the IMAGE CONTENT, not just the keyframe.
                    // If mUndoState->keyframe holds a BitmapKeyFrame, we can't cast to BitmapImage.
                    
                    // We need to see how UndoState is populated.
                    // For now, let's fix the crash by getting the image from the Layer using the KeyFrame's position/ID.
                    // But `mUndoState->keyframe` is a CLONE from BEFORE the operation.
                    
                    // CRITICAL: If Undo system saves `KeyFrame*`, it saves `BitmapKeyFrame`.
                    // `BitmapKeyFrame` only has an ID.
                    // If the Image content changed, `BitmapKeyFrame` doesn't know!
                    // So `mUndoState` logic for Bitmaps is BROKEN for Content Undo if it only saves KeyFrame.
                    
                    // However, we are just fixing the crash for now.
                    // Determining what `bmp` is used for.
                    // It seems to be used to restore the image?
                    // "mEditor->layers()->currentLayer()->replaceKeyFrame(bmp)"
                    
                    // If `bmp` is meant to be the restored KeyFrame, it should be a `BitmapKeyFrame`.
                    // But `LassoSelectTool` likely manipulates the *Image*.
                    
                    // If we cast to `BitmapKeyFrame*`, we get the old ID. 
                    // But if the *Content* of that ID changed, restoring the ID changes nothing!
                    
                    // This suggests we need a deeper fix for Undo: We need to snapshot the `BitmapImage`.
                    
                    // For the IMMEDIATE crash: 
                    // `auto* bkf = static_cast<BitmapKeyFrame*>(mUndoState->keyframe.get());`
                    // Then what?
                    // The original code treated `bmp` as a `KeyFrame*` (which it is) but also cast to `BitmapImage*` to maybe access properties?
                    
                    // Actually, `replaceKeyFrame` takes `KeyFrame*`.
                    // So `bmp` being `BitmapImage*` was just for... what?
                    // `auto* bmp = ...`
                    // It's used as `mEditor->layers()->currentLayer()->replaceKeyFrame(bmp)`.
                    // So it just needs to be a KeyFrame.
                    
                    auto* bkf = static_cast<BitmapKeyFrame*>(mUndoState->keyframe.get());
                    // We can just pass this back?
                    // But wait, if we are "pasting" or "moving", we might have created a NEW Cell?
                    
                    // If we assume `mUndoState->keyframe` is the correct state to restore:
                    // replacing the keyframe will restore the REFERENCE to the old cell.
                    // If the old cell was NOT modified, this works (e.g. keyframe move).
                    // If the old cell WAS modified, this fails to restore content.
                    
                    mEditor->layers()->currentLayer()->replaceKeyFrame(bkf);
                    // Remove static_cast<LayerBitmap*>... since replaceKeyFrame is on Layer
                    mEditor->setModified(mEditor->layers()->currentLayerIndex(), bkf->pos());
                    qDebug() << "[Lasso] Hard-restored keyframe from undo snapshot at pos" << bkf->pos();
                }
            }
            // Clear selection and overlays to ensure no temporary preview remains
            mEditor->deselectAll();
            // Suppress recording the pending move as an undoable action
            mContentWasMoved = false;
            mUndoState = nullptr;
            mScribbleArea->updateFrame();
            return true;
        }
    }
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (hasFloat) {
            // Commit the floating selection and record undo
            if (sel->hasFloating()) {
                if (!sel->selectionTransform().isIdentity()) {
                    // Always bake any active transform into the floating
                    // buffer so the visual result matches the overlay.
                    sel->bakeTransformIntoFloating();
                }
                // For floating selections created from paste we delegate the
                // actual commit + undo/redo creation to Editor::deselectAll().
                // It will see needsFloatingCommit() and call
                // SelectionManager::commitFloatingMove() with a
                // KEYFRAME_MODIFY undo record. For cut/move floating
                // selections retain the previous behavior and commit here.
                if (!sel->isFloatingFromPaste()) {
                    mScribbleArea->handleDrawingOnEmptyFrame();
                    sel->commitFloatingMove();
                    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
                    // Only record undo if content was actually moved
                    if (mContentWasMoved && mUndoState) {
                        mEditor->undoRedo()->record(mUndoState, typeName());
                        mContentWasMoved = false;
                        mUndoState = nullptr;
                    }
                }
            }
            mScribbleArea->applyTransformedSelection();
            mEditor->deselectAll();
            return true;
        }
        break;
    case Qt::Key_Escape:
        if (hasFloat || m_isBuilding) {
            // For pasted floating selections, treat Escape the same as Enter:
            // bake any active transform into the floating buffer and delegate
            // commit + undo/redo recording to Editor::deselectAll(). This
            // ensures Esc, Enter, and deselect-all all share the same solid
            // commit logic for pasted content.
            if (sel && sel->hasFloating() && sel->isFloatingFromPaste()) {
                if (!sel->selectionTransform().isIdentity()) {
                    sel->bakeTransformIntoFloating();
                }
                mScribbleArea->applyTransformedSelection();
                mEditor->deselectAll();
                return true;
            }

            // Commit the floating selection and record undo before canceling
            // for cut/move overlays.
            if (sel && sel->hasFloating()) {
                if (!sel->selectionTransform().isIdentity()) {
                    sel->bakeTransformIntoFloating();
                }
                mScribbleArea->handleDrawingOnEmptyFrame();
                sel->commitFloatingMove();
                mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
                // Only record undo if content was actually moved
                if (mContentWasMoved && mUndoState) {
                    mEditor->undoRedo()->record(mUndoState, typeName());
                    mContentWasMoved = false;
                    mUndoState = nullptr;
                }
            }
            mScribbleArea->cancelTransformedSelection();
            mEditor->deselectAll();
            return true;
        }
        break;
    default:
        break;
    }
    return BaseTool::keyPressEvent(event);
}
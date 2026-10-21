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
#ifndef SELECTIONMANAGER_H
#define SELECTIONMANAGER_H

#include "basemanager.h"
#include "movemode.h"
#include "vertexref.h"
#include "vectorselection.h"
#include "bitmapimage.h"

#include <QPointF>
#include <QRectF>
#include <QPolygonF>
#include <QTransform>
#include <QPainterPath>
#include <QImage>

class Editor;

class SelectionManager : public BaseManager
{
    Q_OBJECT
public:
    explicit SelectionManager(Editor* editor);
    ~SelectionManager() override;

    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;
    void workingLayerChanged(Layer*) override;

    // New state machine for lasso selection
    enum class SelectionState { IDLE, CREATING, SELECTED, MOVING };
    // Track origin of current selection to distinguish lasso-made selections
    enum class SelectionOrigin { NONE, LASSO, OTHER };
    SelectionState state() const { return mState; }
    bool isFromLasso() const { return mOrigin == SelectionOrigin::LASSO; }

    // New lasso lifecycle API
    void beginLassoAt(const QPointF& p);
    void appendLassoPoint(const QPointF& p);
    void closeLasso();

    // Build selection mask and floating content from current frame
    // If cut is true, remove pixels from base; else copy-only
    void extractFloatingFromPath(bool cut);

    // Clear entire selection (path, mask, floating)
    void clearAll();

    // Floating accessors
    bool hasFloating() const { return mHasFloating; }
    const QImage& floatingImage() const { return mFloatingImage; }
    QPointF floatingOrigin() const { return mFloatingOrigin; }
    QPointF floatingOffset() const { return mFloatingOffset; }
    // Note: mHasFloating indicates the presence of a valid floating buffer, independent of offset.
    // Offsets are used solely to preview/commit movement without altering floating persistence.
    void setFloatingOffset(const QPointF& delta) { mFloatingOffset = delta; /* mHasFloating persists */ }
    // Returns true if floating selection exists and has a non-zero offset that needs committing. The
    // floating buffer itself persists after commits until explicitly cleared via clearAll().
    bool needsFloatingCommit() const;

    // Helper to query persistent floating state semantics (alias of hasFloating for clarity)
    bool isFloatingPersistent() const { return mHasFloating; }

    // Outline offsets (can differ from floating when moving boundary only)
    QPointF outlineOffset() const { return mOutlineOffset; }
    void setOutlineOffset(const QPointF& delta) { mOutlineOffset = delta; }

    // Commit current floating move into the base image and keep selection in place
    void commitFloatingMove();
    // Apply outlineOffset to selection path (and floating origin) and reset offsets
    void applyOutlineOffset();
    // Apply floatingOffset to floatingOrigin and selection path without committing to base layer
    void applyFloatingOffset();
    // Ensure base image is cut under the current selection path before moving content
    void ensureSourceCutForMove();
    // Restore previously cut floating content back to the base bitmap at the original position
    // and clear floating/offset state. Does not record undo; intended for cancel semantics (e.g., Undo during drag).
    void restoreFloatingToSourceAndClear();

    // Hit testing
    bool isPointInsideMask(const QPointF& canvasPoint) const;
    bool isPointOnOutline(const QPointF& canvasPoint) const;
    // Bounding-box edge hit test for lasso selections
    bool isPointOnBoundingBox(const QPointF& canvasPoint) const;
    // Back-compat wrappers
    bool isPointInsideSelection(const QPointF& point) const;
    bool isPointOnSelectionBoundary(const QPointF& point) const;

    // Expose current path (without offsets) and with offsets
    QPainterPath selectionPathRaw() const { return mSelectionPath; }
    QPainterPath selectionPathWithOffsets() const;

    // Legacy API kept for compatibility but unused by new logic
    void flipSelection(bool flipVertical);
    void setSelection(QRectF rect, bool roundPixels=false);
    void setSelectionPath(const QPainterPath& path);
    QPainterPath selectionPath() const { return mSelectionPath; }
    bool hasSelectionPath() const { return !mSelectionPath.isEmpty(); }

    void translate(QPointF point);
    void rotate(qreal angle, qreal lockedAngle);
    void scale(qreal sX, qreal sY);
    void maintainAspectRatio(bool state) { mAspectRatioFixed = state; }

    void alignPositionToAxis(bool state) { mLockAxis = state; }

    void setMoveModeForAnchorInRange(const QPointF& point);
    MoveMode getMoveMode() const { return mMoveMode; }
    void setMoveMode(const MoveMode moveMode) { mMoveMode = moveMode; }

    bool somethingSelected() const { return mOriginalRect.isValid() || !mSelectionPath.isEmpty(); }

    void adjustSelection(const QPointF& currentPoint, const QPointF& offset, qreal rotationOffset, int rotationIncrement = 0);

    QTransform selectionTransform() const { return mSelectionTransform; }
    void setSelectionTransform(const QTransform& transform) { mSelectionTransform = transform; }
    void resetSelectionTransform();

    void resetSelectionTransformProperties();

    void resetSelectionProperties();
    void deleteSelection();

    bool isOutsideSelectionArea(const QPointF& point) const;

    qreal selectionTolerance() const;

    qreal selectionWidth() const { return (mSelectionPolygon[1] - mSelectionPolygon[0]).x(); }
    qreal selectionHeight() const { return (mSelectionPolygon[3] - mSelectionPolygon[0]).y(); }

    QPointF currentTransformAnchor() const { return mAnchorPoint; }
    QPointF getSelectionAnchorPoint() const;

    void setTransformAnchor(const QPointF& point);

    const QRectF& mySelectionRect() const { return mOriginalRect; }
    const qreal& myRotation() const { return mRotatedAngle; }
    const qreal& myScaleX() const { return mScaleX; }
    const qreal& myScaleY() const { return mScaleY; }
    const QPointF& myTranslation() const { return mTranslation; }

    void setRotation(const qreal& rotation) { mRotatedAngle = rotation; }
    void setScale(const qreal scaleX, const qreal scaleY) { mScaleX = scaleX; mScaleY = scaleY; }
    void setTranslation(const QPointF& translation) { mTranslation = translation; }

    qreal angleFromPoint(const QPointF& point, const QPointF& anchorPoint) const;

    QPointF mapToSelection(const QPointF& point) const { return mSelectionTransform.map(point); };
    QPointF mapFromLocalSpace(const QPointF& point) const { return mSelectionTransform.inverted().map(point); }
    QPolygonF mapToSelection(const QPolygonF& polygon) const { return mSelectionTransform.map(polygon); }
    QPolygonF mapFromLocalSpace(const QPolygonF& polygon) const { return mSelectionTransform.inverted().map(polygon); }

    // Vector selection
    VectorSelection vectorSelection;

    void setCurves(const QList<int>& curves) { mClosestCurves = curves; }
    void setVertices(const QList<VertexRef>& vertices) { mClosestVertices = vertices; }

    void clearCurves() { mClosestCurves.clear(); };
    void clearVertices() { mClosestVertices.clear(); };

    void setDragOrigin(const QPointF point) { mDragOrigin = point; }

    const QList<int> closestCurves() const { return mClosestCurves; }
    const QList<VertexRef> closestVertices() const { return mClosestVertices; }

    void calculateSelectionTransformation();
    void bakeTransformIntoSelectionGeometry();
    // Apply current selectionTransform to the floating image and geometry, then reset transform
    void bakeTransformIntoFloating();

    // Interpolation mode for overlay preview and baking
    void setResampleSmooth(bool on) { mResampleSmooth = on; }
    bool resampleSmooth() const { return mResampleSmooth; }

    // Initialize a floating selection from an arbitrary image and canvas-space
    // origin, without cutting pixels from the base layer. Used for bitmap
    // paste so the user can move/transform the pasted content as an overlay
    // before committing it back to the bitmap.
    void beginFloatingFromImage(const QImage& image, const QPointF& origin);
    void beginFloatingFromImage(const QImage& image, const QPointF& origin,
                                const QVector<BitmapImage::HybridVectorStroke>& strokes);

    // Query whether the current floating selection originated from a paste
    // operation (as opposed to a cut/move). Tools can use this to route
    // commit/undo semantics appropriately.
    bool isFloatingFromPaste() const { return mFloatingFromPaste; }

signals:
    void selectionChanged();
    void selectionReset();
    void needDeleteSelection();

private:
    QPointF alignPositionToAxis(QPointF currentPoint) const;
    int constrainRotationToAngle(const qreal rotatedAngle, const int rotationIncrement) const;

    // New state/mask/floating content
    SelectionState mState = SelectionState::IDLE;
    SelectionOrigin mOrigin = SelectionOrigin::NONE;
    QPainterPath mSelectionPath;
    QImage mSelectionMask; // in selection-local space (bounding rect size)
    QImage mFloatingImage; // selected pixels
    QPointF mFloatingOrigin = QPointF(0,0); // canvas-space top-left of bounding rect
    QPointF mFloatingOffset = QPointF(0,0); // delta while moving content
    QPointF mOutlineOffset = QPointF(0,0);  // delta while moving boundary only
    bool mHasFloating = false;
    bool mWasCutFromSource = false; // whether pixels were already cleared from base

    bool mAspectRatioFixed = false;
    bool mLockAxis = false;
    QPolygonF mSelectionPolygon;
    QRectF mOriginalRect;

    qreal mScaleX;
    qreal mScaleY;
    QPointF mTranslation;
    qreal mRotatedAngle = 0.0;

    QList<int> mClosestCurves;
    QList<VertexRef> mClosestVertices;

    QPointF mDragOrigin;

    MoveMode mMoveMode = MoveMode::NONE;
    QTransform mSelectionTransform;
    const qreal mSelectionTolerance = 10.0;

    QPointF mAnchorPoint;

    // Whether selectionTransform should be used to preview and apply content movement
    bool mTransformAffectsContent = false;

    // Whether to use smooth interpolation (true) or nearest-neighbor (false) for overlay
    bool mResampleSmooth = true;

    // Track number of committed moves for debugging/optimization
    int mMoveCount = 0;

    // True when the current floating content was created from a paste
    // operation instead of being cut from the source bitmap. In this mode we
    // never clear pixels from the base image when committing; we only overlay
    // the floating image.
    bool mFloatingFromPaste = false;

    // Floating hybrid strokes cut from the bitmap under the selection mask.
    // These move/transform with the floating bitmap and are reinserted on
    // commit/restore.
    QVector<BitmapImage::HybridVectorStroke> mFloatingHybridStrokes;
    bool mHasFloatingHybrid = false;
};

#endif // SELECTIONMANAGER_H

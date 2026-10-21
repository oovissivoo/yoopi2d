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
#include "selectionmanager.h"
#include "editor.h"

#include "vectorimage.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "bitmapkeyframe.h"
#include "structure/celllayerstack.h"
#include "layermanager.h"

#include "mathutils.h"

#include <QVector2D>
#include <QPainterPathStroker>
#include <QtGlobal>
#include <QDebug>


SelectionManager::SelectionManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
}

void SelectionManager::bakeTransformIntoFloating()
{
    if (!mHasFloating) return;

    // Map selection path by current transform to update geometry
    if (!mSelectionPath.isEmpty()) {
        mSelectionPath = mSelectionTransform.map(mSelectionPath);
        mOriginalRect = mSelectionPath.boundingRect();
        mSelectionPolygon = mOriginalRect;
    } else if (!mSelectionPolygon.isEmpty()) {
        QPolygonF mapped = mSelectionTransform.map(mSelectionPolygon);
        mSelectionPolygon = mapped;
        mOriginalRect = mapped.boundingRect();
    }

    // Bake the full selection transform (scale, rotate, translate) into the floating image
    // Compute destination canvas bounding rect by transforming the current floating rect
    const QImage src = mFloatingImage;
    QRectF srcCanvasRect(mFloatingOrigin, src.size());
    QPolygonF mappedRect = mSelectionTransform.map(QPolygonF(srcCanvasRect));
    QRect destCanvasRect = mappedRect.boundingRect().toAlignedRect();
    if (destCanvasRect.isEmpty()) {
        destCanvasRect = QRect(0,0,1,1);
    }

    QImage dst(destCanvasRect.size(), QImage::Format_ARGB32_Premultiplied);
    dst.fill(Qt::transparent);
    {
        QPainter p(&dst);
        // Only enable smooth/AA when the user has opted into resampling.
        // With AA off we want nearest-neighbour, hard-edge pixels — no fringe.
        p.setRenderHint(QPainter::Antialiasing, mResampleSmooth);
        p.setRenderHint(QPainter::SmoothPixmapTransform, mResampleSmooth);
        // Align painter so that drawing in canvas space maps into dst [0,0]
        p.translate(-destCanvasRect.topLeft());
        p.setWorldTransform(mSelectionTransform, true);
        // Use drawPixmap so SmoothPixmapTransform is honoured (drawImage ignores it).
        p.drawPixmap(mFloatingOrigin.toPoint(), QPixmap::fromImage(src));
        p.end();
    }

    mFloatingImage = dst;
    mFloatingOrigin = destCanvasRect.topLeft();

    // Apply the same transform to floating hybrid strokes so their points
    // remain in sync with the transformed bitmap content.
    if (mHasFloatingHybrid) {
        for (BitmapImage::HybridVectorStroke& stroke : mFloatingHybridStrokes) {
            for (QPointF& p : stroke.points) {
                p = mSelectionTransform.map(p);
            }
        }
    }

    // Reset offsets and transform
    mFloatingOffset = QPointF(0,0);
    mOutlineOffset = QPointF(0,0);
    resetSelectionTransformProperties();
    calculateSelectionTransformation();
    emit selectionChanged();
}

void SelectionManager::beginFloatingFromImage(const QImage& image, const QPointF& origin,
                                              const QVector<BitmapImage::HybridVectorStroke>& strokes)
{
    beginFloatingFromImage(image, origin);
    if (image.isNull()) {
        return;
    }
    mFloatingHybridStrokes = strokes;
    mHasFloatingHybrid = !mFloatingHybridStrokes.isEmpty();
}

void SelectionManager::beginFloatingFromImage(const QImage& image, const QPointF& origin)
{
    if (image.isNull()) {
        return;
    }

    // Reset transform and selection geometry to a simple rectangle that
    // matches the pasted image bounds at the given canvas origin.
    resetSelectionTransformProperties();

    mFloatingImage = image;
    mFloatingOrigin = origin;
    mFloatingOffset = QPointF(0,0);
    mOutlineOffset = QPointF(0,0);

    QRectF rect(origin, image.size());
    mSelectionPath = QPainterPath();
    mSelectionPath.addRect(rect);
    mSelectionPolygon = rect;
    mOriginalRect = rect;

    // No mask and no source cut: this is a pure overlay until committed.
    mSelectionMask = QImage();
    mHasFloating = true;
    mWasCutFromSource = false;
    mFloatingFromPaste = true;
    mState = SelectionState::SELECTED;

    calculateSelectionTransformation();
    emit selectionChanged();
}

void SelectionManager::restoreFloatingToSourceAndClear()
{
    if (!mHasFloating) return;
    qDebug() << "[Selection] restoreFloatingToSourceAndClear: hasFloating size=" << mFloatingImage.size()
             << " origin=" << mFloatingOrigin << " maskSize=" << mSelectionMask.size();
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) {
        // Clear floating state even if we can't paste back
        mFloatingOffset = QPointF(0,0);
        mOutlineOffset = QPointF(0,0);
        mHasFloating = false;
        mWasCutFromSource = false;
        emit selectionChanged();
        return;
    }
    // If source was never cut (copy-only overlay), just clear overlay without pasting to avoid duplication
    if (!mWasCutFromSource) {
        mFloatingOffset = QPointF(0,0);
        mOutlineOffset = QPointF(0,0);
        mHasFloating = false;
        // Keep selection path; remain selected
        emit selectionChanged();
        return;
    }
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);
    // Restore to the active inner layer (same layer the content was cut from)
    const int frameNumber = editor()->currentFrame();
    BitmapImage* img = nullptr;
    CellLayerStack* restoreStack = nullptr;
    {
        KeyFrame* kf = bitmapLayer->keyExists(frameNumber)
                       ? bitmapLayer->getKeyFrameAt(frameNumber)
                       : bitmapLayer->getLastKeyFrameAtPosition(frameNumber);
        if (kf) {
            BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
            CellBank* bank = bitmapLayer->cellBank();
            img = bank->getActiveInnerLayerImage(bkf->cellId());
            restoreStack = bank->getCellLayerStack(bkf->cellId());
        }
    }
    if (!img) {
        img = bitmapLayer->getLastBitmapImageAtFrame(frameNumber);
    }
    if (img && !img->image()->isNull()) {
        // Destination top-left in canvas space: original floating origin (ignore preview offsets)
        QPointF canvasDestTopLeftF = mFloatingOrigin;
        // Compute target in image-local coordinates based on current image topLeft (may have shifted due to auto-crop)
        QPoint targetTopLeft(qRound(canvasDestTopLeftF.x() - img->topLeft().x()),
                             qRound(canvasDestTopLeftF.y() - img->topLeft().y()));

        // Ensure destination area exists in current image bounds; grow without scaling if needed
        QRect canvasDestRect(canvasDestTopLeftF.toPoint(), mFloatingImage.size());
        if (!img->bounds().contains(canvasDestRect)) {
            qDebug() << "[Selection] restore: growing bounds from" << img->bounds() << " to include" << canvasDestRect
                     << " imgTopLeft(before)=" << img->topLeft();
            // Temporarily disable auto-crop to avoid shrinking around paste
            bool autoCropWasEnabled = img->isMinimallyBounded();
            img->enableAutoCrop(false);
            img->growToInclude(canvasDestRect);
            // Recompute target after bounds change
            targetTopLeft = QPoint(qRound(canvasDestTopLeftF.x() - img->topLeft().x()),
                                   qRound(canvasDestTopLeftF.y() - img->topLeft().y()));
            // Paste
            QPainter pastePainter(img->image());
            pastePainter.setRenderHint(QPainter::Antialiasing, false);
            pastePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            pastePainter.drawImage(targetTopLeft, mFloatingImage);
            pastePainter.end();
            img->enableAutoCrop(autoCropWasEnabled);
            qDebug() << "[Selection] restore: pasted at" << targetTopLeft << " imgTopLeft(after)=" << img->topLeft();
        } else {
            // Within bounds; paste directly
            QPainter pastePainter(img->image());
            pastePainter.setRenderHint(QPainter::Antialiasing, false);
            pastePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            pastePainter.drawImage(targetTopLeft, mFloatingImage);
            pastePainter.end();
            qDebug() << "[Selection] restore: pasted at" << targetTopLeft << " within bounds";
        }

        // Reinsert any floating hybrid strokes back into the bitmap so
        // snapping paths are restored together with the pixels.
        if (mHasFloatingHybrid) {
            for (const BitmapImage::HybridVectorStroke& stroke : mFloatingHybridStrokes) {
                if (stroke.points.size() >= 2) {
                    img->addHybridStroke(stroke.points, stroke.baseWidth);
                }
            }
        }

        // Mark modified for visibility/dirty tracking
        editor()->setModified(editor()->layers()->currentLayerIndex(), editor()->currentFrame());
    }

    // Invalidate composite so CanvasPainter recomputes it on next render (Req 8.1)
    if (restoreStack) restoreStack->invalidateComposite();

    // Clear floating/offset state and keep the selection path (remain selected)
    mFloatingOffset = QPointF(0,0);
    mOutlineOffset = QPointF(0,0);
    mHasFloating = false;
    mWasCutFromSource = false;
    mFloatingHybridStrokes.clear();
    mHasFloatingHybrid = false;
    mState = SelectionState::SELECTED;
    emit selectionChanged();
}

SelectionManager::~SelectionManager()
{
}

bool SelectionManager::init()
{
    return true;
}

Status SelectionManager::load(Object*)
{
    resetSelectionProperties();
    return Status::OK;
}

Status SelectionManager::save(Object*)
{
    return Status::OK;
}

void SelectionManager::workingLayerChanged(Layer *)
{
}

void SelectionManager::resetSelectionTransformProperties()
{
    mRotatedAngle = 0;
    mTranslation = QPointF(0, 0);
    mScaleX = 1;
    mScaleY = 1;
    mAnchorPoint = QPoint();
    mSelectionTransform.reset();
}

void SelectionManager::resetSelectionTransform()
{
    mSelectionTransform.reset();
}

bool SelectionManager::isOutsideSelectionArea(const QPointF& point) const
{
    if (!mSelectionPath.isEmpty()) {
        QPainterPath projected = mSelectionTransform.map(mSelectionPath);
        return (!projected.contains(point)) && mMoveMode == MoveMode::NONE;
    }
    return (!mSelectionTransform.map(mSelectionPolygon).containsPoint(point, Qt::WindingFill)) && mMoveMode == MoveMode::NONE;
}

bool SelectionManager::isPointInsideSelection(const QPointF& point) const
{
    return isPointInsideMask(point);
}

bool SelectionManager::isPointOnSelectionBoundary(const QPointF& point) const
{
    return isPointOnOutline(point);
}

void SelectionManager::deleteSelection()
{
    emit needDeleteSelection();
}

qreal SelectionManager::selectionTolerance() const
{
    return qAbs(mSelectionTolerance * editor()->viewScaleInversed());
}

QPointF SelectionManager::getSelectionAnchorPoint() const
{
    QPointF anchorPoint;
    if (mSelectionPolygon.count() < 3) { return anchorPoint; }

    if (mMoveMode == MoveMode::BOTTOMRIGHT)
    {
        anchorPoint = mSelectionPolygon[0];
    }
    else if (mMoveMode == MoveMode::BOTTOMLEFT)
    {
        anchorPoint = mSelectionPolygon[1];
    }
    else if (mMoveMode == MoveMode::TOPLEFT)
    {
        anchorPoint = mSelectionPolygon[2];
    }
    else if (mMoveMode == MoveMode::TOPRIGHT)
    {
        anchorPoint = mSelectionPolygon[3];
    } else {
        anchorPoint = QLineF(mSelectionPolygon[0], mSelectionPolygon[2]).pointAt(.5);
    }
    return anchorPoint;
}


void SelectionManager::setMoveModeForAnchorInRange(const QPointF& point)
{
    if (mSelectionPolygon.count() < 4)
    {
        mMoveMode = MoveMode::NONE;
        return;
    }

    QPolygonF projectedPolygon = mapToSelection(mSelectionPolygon);

    // If we have a path selection, consider inside path as MIDDLE
    if (!mSelectionPath.isEmpty()) {
        QPainterPath pathProjected = mSelectionTransform.map(mSelectionPath);
        if (pathProjected.contains(point)) {
            mMoveMode = MoveMode::MIDDLE;
            return;
        }
    }

    const double calculatedSelectionTol = selectionTolerance();

    if (QLineF(point, projectedPolygon[0]).length() < calculatedSelectionTol)
    {
        mMoveMode = MoveMode::TOPLEFT;
    }
    else if (QLineF(point, projectedPolygon[1]).length() < calculatedSelectionTol)
    {
        mMoveMode = MoveMode::TOPRIGHT;
    }
    else if (QLineF(point, projectedPolygon[2]).length() < calculatedSelectionTol)
    {
        mMoveMode = MoveMode::BOTTOMRIGHT;
    }
    else if (QLineF(point, projectedPolygon[3]).length() < calculatedSelectionTol)
    {
        mMoveMode = MoveMode::BOTTOMLEFT;
    }
    else if (projectedPolygon.containsPoint(point, Qt::WindingFill))
    {
        mMoveMode = MoveMode::MIDDLE;
    }
    else
    {
        mMoveMode = MoveMode::NONE;
    }
}

void SelectionManager::adjustSelection(const QPointF& currentPoint, const QPointF& offset, qreal rotationOffset, int rotationIncrement)
{
    switch (mMoveMode)
    {
    case MoveMode::MIDDLE: {
        QPointF newOffset = currentPoint - mDragOrigin;

        if (mLockAxis) {
            mTranslation = offset + alignPositionToAxis(newOffset);
        } else {
            mTranslation = offset + newOffset;
        }
        break;
    }
    case MoveMode::TOPLEFT:
    case MoveMode::TOPRIGHT:
    case MoveMode::BOTTOMRIGHT:
    case MoveMode::BOTTOMLEFT: {

        QPolygonF projectedPolygon = mapToSelection(mSelectionPolygon);
        QVector2D currentPVec = QVector2D(currentPoint);

        qreal originWidth = mSelectionPolygon[1].x() - mSelectionPolygon[0].x();
        qreal originHeight = mSelectionPolygon[3].y() - mSelectionPolygon[0].y();

        QVector2D staticXAnchor;
        QVector2D staticYAnchor;
        QVector2D movingAnchor;
        if (mMoveMode == MoveMode::TOPLEFT) {
            movingAnchor = QVector2D(projectedPolygon[0]);
            staticXAnchor = QVector2D(projectedPolygon[1]);
            staticYAnchor = QVector2D(projectedPolygon[3]);
        } else if (mMoveMode == MoveMode::TOPRIGHT) {
            movingAnchor = QVector2D(projectedPolygon[1]);
            staticXAnchor = QVector2D(projectedPolygon[0]);
            staticYAnchor = QVector2D(projectedPolygon[2]);
        } else if (mMoveMode == MoveMode::BOTTOMRIGHT) {
            movingAnchor = QVector2D(projectedPolygon[2]);
            staticXAnchor = QVector2D(projectedPolygon[3]);
            staticYAnchor = QVector2D(projectedPolygon[1]);
        } else {
            movingAnchor = QVector2D(projectedPolygon[3]);
            staticXAnchor = QVector2D(projectedPolygon[2]);
            staticYAnchor = QVector2D(projectedPolygon[0]);
        }

        QVector2D directionVecX = staticXAnchor - currentPVec;
        QVector2D directionVecY = staticYAnchor - currentPVec;

        // Calculates the signed distance
        qreal distanceX = QVector2D::dotProduct(directionVecX, (staticXAnchor - movingAnchor).normalized());
        qreal distanceY = QVector2D::dotProduct(directionVecY, (staticYAnchor - movingAnchor).normalized());

        qreal scaleX = distanceX / originWidth;
        qreal scaleY = distanceY / originHeight;
        if (mAspectRatioFixed) {
            scaleY = scaleX;
        }

        scale(scaleX, scaleY);

        break;
    }
    case MoveMode::ROTATION: {
        rotate(rotationOffset, rotationIncrement);
        break;
    }
    default:
        break;
    }
    calculateSelectionTransformation();
}

void SelectionManager::translate(QPointF newPos)
{
    mTranslation += newPos;
}

void SelectionManager::rotate(qreal angle, qreal lockedAngle)
{
    if (lockedAngle > 0) {
        mRotatedAngle = constrainRotationToAngle(angle, lockedAngle);
    } else {
        mRotatedAngle = angle;
    }
}

void SelectionManager::scale(qreal sX, qreal sY)
{
    // Enforce negative scaling when
    // deliberately trying to transform in negative space
    if (mScaleX < 0) {
        sX = -sX;
    }
    if (qFuzzyIsNull(sX)) {
        // Scale must not become 0
        sX = 0.0001;
    }

    // Enforce negative scaling when
    // deliberately trying to transform in negative space
    if (mScaleY < 0) {
        sY = -sY;
    }
    if (qFuzzyIsNull(sY)) {
        // Scale must not become 0
        sY = 0.0001;
    }

    mScaleX = sX;
    mScaleY = sY;
}

int SelectionManager::constrainRotationToAngle(const qreal rotatedAngle, const int rotationIncrement) const
{
    return qRound(rotatedAngle / rotationIncrement) * rotationIncrement;
}

qreal SelectionManager::angleFromPoint(const QPointF& point, const QPointF& anchorPoint) const
{
    return qRadiansToDegrees(MathUtils::getDifferenceAngle(mSelectionTransform.map(anchorPoint), point));
}

void SelectionManager::setSelection(QRectF rect, bool roundPixels)
{
    resetSelectionTransformProperties();
    mSelectionPath = QPainterPath();
    if (roundPixels)
    {
        rect = rect.toAlignedRect();
    }
    mSelectionPolygon = rect;
    mOriginalRect = rect;
    mScaleX = 1;
    mScaleY = 1;
    mRotatedAngle = 0;
    mTransformAffectsContent = false;
    mOrigin = SelectionOrigin::OTHER;

    emit selectionChanged();
}

void SelectionManager::setSelectionPath(const QPainterPath& path)
{
    resetSelectionTransformProperties();
    mSelectionPath = path;
    QRectF rect = mSelectionPath.boundingRect();
    mSelectionPolygon = rect;
    mOriginalRect = rect;
    mScaleX = 1;
    mScaleY = 1;
    mRotatedAngle = 0;
    mTransformAffectsContent = false;
    mOrigin = SelectionOrigin::OTHER;
    emit selectionChanged();
}

void SelectionManager::setTransformAnchor(const QPointF& point)
{
    QPointF newPos = mapToSelection(point);
    QPointF oldPos = mapToSelection(mAnchorPoint);

    // Adjust translation based on anchor point to avoid moving the selection
    mTranslation = mTranslation - oldPos + newPos;
    mAnchorPoint = point;
}

void SelectionManager::calculateSelectionTransformation()
{
    QTransform t;
    t.translate(-mAnchorPoint.x(), -mAnchorPoint.y());
    QTransform t2;
    t2.translate(mTranslation.x(), mTranslation.y());

    QTransform r;
    r.rotate(mRotatedAngle);
    QTransform s;
    s.scale(mScaleX, mScaleY);
    mSelectionTransform = t * s * r * t2;
}

void SelectionManager::bakeTransformIntoSelectionGeometry()
{
    if (mOriginalRect.isNull()) { return; }
    // Map current geometry by the selection transform
    if (!mSelectionPath.isEmpty()) {
        mSelectionPath = mSelectionTransform.map(mSelectionPath);
        mOriginalRect = mSelectionPath.boundingRect();
        mSelectionPolygon = mOriginalRect;
    } else {
        QPolygonF mapped = mSelectionTransform.map(mSelectionPolygon);
        mSelectionPolygon = mapped;
        mOriginalRect = mapped.boundingRect();
    }
    // Reset transform so it is identity afterward
    resetSelectionTransformProperties();
    calculateSelectionTransformation();
    emit selectionChanged();
}

QPointF SelectionManager::alignPositionToAxis(QPointF currentPoint) const
{
    if (qAbs(currentPoint.y()) > qAbs(currentPoint.x())) {
        // Align to y axis
        return QPointF(0, currentPoint.y());
    }

    // Align to x axis
    return QPointF(currentPoint.x(), 0);
}

/**
 * @brief ScribbleArea::flipSelection
 * flip selection along the X or Y axis
*/
void SelectionManager::flipSelection(bool flipVertical)
{
    if (flipVertical)
    {
        mScaleY = -mScaleY;
    }
    else
    {
        mScaleX = -mScaleX;
    }
    setTransformAnchor(mOriginalRect.center());
    calculateSelectionTransformation();
    emit selectionChanged();
}

void SelectionManager::resetSelectionProperties()
{
    resetSelectionTransformProperties();
    mSelectionPolygon = QPolygonF();
    mOriginalRect = QRectF();
    mSelectionPath = QPainterPath();
    mTransformAffectsContent = false;
    mOrigin = SelectionOrigin::NONE;
    emit selectionChanged();
}

void SelectionManager::beginLassoAt(const QPointF& p)
{
    mSelectionPath = QPainterPath(p);
    mOriginalRect = QRectF(p, QSizeF(1,1));
    mSelectionPolygon = mOriginalRect;
    mSelectionMask = QImage();
    mFloatingImage = QImage();
    mFloatingOrigin = p;
    mFloatingOffset = QPointF(0,0);
    mOutlineOffset = QPointF(0,0);
    mHasFloating = false;
    mWasCutFromSource = false;
    mState = SelectionState::CREATING;
    mOrigin = SelectionOrigin::LASSO;
    emit selectionChanged();
}

void SelectionManager::appendLassoPoint(const QPointF& p)
{
    if (mState != SelectionState::CREATING) return;
    if (QLineF(mSelectionPath.currentPosition(), p).length() < 0.1) return;
    mSelectionPath.lineTo(p);
    mOriginalRect = mSelectionPath.boundingRect();
    mSelectionPolygon = mOriginalRect;
    emit selectionChanged();
}

void SelectionManager::closeLasso()
{
    if (mState != SelectionState::CREATING) return;
    mSelectionPath.closeSubpath();
    mOriginalRect = mSelectionPath.boundingRect();
    mSelectionPolygon = mOriginalRect;
    mState = SelectionState::SELECTED;
    emit selectionChanged();
}

void SelectionManager::extractFloatingFromPath(bool cut)
{
    if (mSelectionPath.isEmpty()) return;
    // Build mask in local (bounding) space
    QRect bbox = mSelectionPath.boundingRect().toAlignedRect();
    if (bbox.isEmpty()) return;
    mSelectionMask = QImage(bbox.size(), QImage::Format_ARGB32_Premultiplied);
    mSelectionMask.fill(Qt::transparent);
    {
        QPainter maskPainter(&mSelectionMask);
        QPainterPath local = mSelectionPath;
        local.translate(-bbox.topLeft());
        maskPainter.fillPath(local, Qt::white);
    }

    // Extract pixels from current layer bitmap (vector not handled here)
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) {
        mHasFloating = false;
        mWasCutFromSource = false;
        return;
    }
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);

    // Task 8.1 (Req 3.7): Floating selection is sourced from the Active_Inner_Layer
    const int frameNumber = editor()->currentFrame();
    BitmapImage* img = nullptr;
    {
        KeyFrame* kf = bitmapLayer->keyExists(frameNumber)
                       ? bitmapLayer->getKeyFrameAt(frameNumber)
                       : bitmapLayer->getLastKeyFrameAtPosition(frameNumber);
        if (kf) {
            BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
            img = bitmapLayer->cellBank()->getActiveInnerLayerImage(bkf->cellId());
        }
    }
    if (!img) {
        img = bitmapLayer->getLastBitmapImageAtFrame(frameNumber);
    }
    if (!img) { mHasFloating = false; return; }

    mFloatingOrigin = bbox.topLeft();
    // Copy from image-local coordinates using the canvas-space bbox
    QRect localRect = QRect((mFloatingOrigin - img->topLeft()).toPoint(), bbox.size());
    QImage sub = img->image()->copy(localRect);

    // Apply mask to subimage to get exact selected pixels
    QImage masked(sub.size(), QImage::Format_ARGB32_Premultiplied);
    masked.fill(Qt::transparent);
    {
        QPainter p(&masked);
        p.drawImage(QPoint(0,0), sub);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.drawImage(QPoint(0,0), mSelectionMask);
    }
    mFloatingImage = masked;

    if (cut) {
        // Clear using the exact selection mask for pixel-perfect removal and
        // extract floating hybrid strokes from the bitmap.
        ensureSourceCutForMove();
    } else {
        mWasCutFromSource = false;
        mFloatingHybridStrokes.clear();
        mHasFloatingHybrid = false;
    }

    mFloatingOffset = QPointF(0,0);
    mOutlineOffset = QPointF(0,0);
    mHasFloating = true;
    mFloatingFromPaste = false;
    mState = SelectionState::SELECTED;
    // Notify canvas to refresh caches and redraw after cut/copy
    editor()->setModified(editor()->layers()->currentLayerIndex(), editor()->currentFrame());
    emit selectionChanged();
}

void SelectionManager::ensureSourceCutForMove()
{
    if (mWasCutFromSource) return;
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);

    // Cut from the active inner layer (Req 8.1, 3.6, 3.7)
    const int frameNumber = editor()->currentFrame();
    BitmapImage* img = nullptr;
    CellLayerStack* cutStack = nullptr;
    {
        KeyFrame* kf = bitmapLayer->keyExists(frameNumber)
                       ? bitmapLayer->getKeyFrameAt(frameNumber)
                       : bitmapLayer->getLastKeyFrameAtPosition(frameNumber);
        if (kf) {
            BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
            CellBank* bank = bitmapLayer->cellBank();
            img = bank->getActiveInnerLayerImage(bkf->cellId());
            cutStack = bank->getCellLayerStack(bkf->cellId());
        }
    }
    if (!img) {
        img = bitmapLayer->getLastBitmapImageAtFrame(frameNumber);
    }
    if (!img || img->image()->isNull()) return;

    if (mSelectionMask.isNull()) return;

    QPoint srcTopLeft = (mFloatingOrigin - img->topLeft()).toPoint();
    QPainter clearPainter(img->image());
    clearPainter.setRenderHint(QPainter::Antialiasing, false);
    clearPainter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    clearPainter.drawImage(srcTopLeft, mSelectionMask);

    mFloatingHybridStrokes.clear();
    img->clipHybridStrokesWithMask(mSelectionMask, mFloatingOrigin, &mFloatingHybridStrokes);
    mHasFloatingHybrid = !mFloatingHybridStrokes.isEmpty();
    mWasCutFromSource = true;

    // Invalidate composite after cut (Req 8.1)
    if (cutStack) cutStack->invalidateComposite();
}

void SelectionManager::commitFloatingMove()
{
    if (!mHasFloating) return;
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return;
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);

    // Task 8.1 (Req 8.1): Paste/floating-move commits to the Active_Inner_Layer,
    // not the composite. This ensures the paste writes only to the currently
    // selected inner layer and leaves all other inner layers intact.
    const int frameNumber = editor()->currentFrame();
    KeyFrame* kf = bitmapLayer->keyExists(frameNumber)
                   ? bitmapLayer->getKeyFrameAt(frameNumber)
                   : bitmapLayer->getLastKeyFrameAtPosition(frameNumber);
    BitmapImage* img = nullptr;
    CellLayerStack* stack = nullptr;
    if (kf) {
        BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
        CellBank* bank = bitmapLayer->cellBank();
        img = bank->getActiveInnerLayerImage(bkf->cellId());
        stack = bank->getCellLayerStack(bkf->cellId());
    }
    if (!img) {
        // Fallback to composite (legacy path) if inner layer API unavailable
        img = bitmapLayer->getLastBitmapImageAtFrame(frameNumber);
    }
    // Allow null mImage (empty keyframe) so we can initialize it via growToInclude
    if (!img) return;

    // Compute target top-left in image-local coordinates, round to nearest pixel
    // Note: floatingOrigin already includes all applied offsets from previous drags
    QPointF canvasDestTopLeftF = mFloatingOrigin;
    QPoint targetTopLeft(qRound(canvasDestTopLeftF.x() - img->topLeft().x()),
                         qRound(canvasDestTopLeftF.y() - img->topLeft().y()));

    // Track dest region in canvas space for downstream cache invalidation
    QRectF dstCanvasRect(mFloatingOrigin, mFloatingImage.size());
    QRectF unionCanvasRect = dstCanvasRect.adjusted(-2, -2, 2, 2);

    // Ensure the source area is cut exactly once (only when a move is committed)
    ensureSourceCutForMove();
    // Cutting can change bitmap bounds (auto-crop), so recompute paste offset
    targetTopLeft = QPoint(qRound(canvasDestTopLeftF.x() - img->topLeft().x()),
                           qRound(canvasDestTopLeftF.y() - img->topLeft().y()));

    // If destination lies outside current bitmap bounds, expand the bitmap without scaling
    QRect canvasDestRect(canvasDestTopLeftF.toPoint(), mFloatingImage.size());
    if (!img->bounds().contains(canvasDestRect)) {
        // Temporarily disable autoCrop to prevent bounds from shrinking after paste
        bool autoCropWasEnabled = img->isMinimallyBounded();
        img->enableAutoCrop(false);
        
        img->growToInclude(canvasDestRect);
        // Recompute target top-left in image-local coordinates after bounds change (topLeft may shift)
        // Note: topLeft() won't trigger autoCrop since we disabled it
        targetTopLeft = QPoint(qRound(canvasDestTopLeftF.x() - img->topLeft().x()),
                               qRound(canvasDestTopLeftF.y() - img->topLeft().y()));
        
        // Paste floating image at new position BEFORE re-enabling autoCrop
        QPainter pastePainter(img->image());
        pastePainter.setRenderHint(QPainter::Antialiasing, false);
        pastePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        pastePainter.drawImage(targetTopLeft, mFloatingImage);
        pastePainter.end();
        
        // Re-enable autoCrop after paste is complete
        img->enableAutoCrop(autoCropWasEnabled);
    } else {
        // Destination is within existing bounds, paste directly
        QPainter pastePainter(img->image());
        pastePainter.setRenderHint(QPainter::Antialiasing, false);
        pastePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        pastePainter.drawImage(targetTopLeft, mFloatingImage);
        pastePainter.end();
    }

    // Reinsert floating hybrid strokes at their final positions so snapping
    // data follows the moved/rotated/scaled selection content.
    if (mHasFloatingHybrid && img) {
        for (const BitmapImage::HybridVectorStroke& stroke : mFloatingHybridStrokes) {
            if (stroke.points.size() >= 2) {
                img->addHybridStroke(stroke.points, stroke.baseWidth);
            }
        }
    }

    // Notify overlay to update after paste
    emit selectionChanged();

    // If selection path is empty (edge case), rebuild a simple rect at the new location
    if (mSelectionPath.isEmpty() && !mSelectionMask.isNull()) {
        QPainterPath rebuilt;
        rebuilt.addRect(QRectF(canvasDestTopLeftF, mSelectionMask.size()));
        mSelectionPath = rebuilt;
        mOriginalRect = mSelectionPath.boundingRect();
        mSelectionPolygon = mOriginalRect;
    }

    // Selection geometry is already at the correct position (floatingOrigin was updated by applyFloatingOffset)
    // Just ensure the rects are synced
    if (!mSelectionPath.isEmpty()) {
        mOriginalRect = mSelectionPath.boundingRect();
        mSelectionPolygon = mOriginalRect;
    }

    // Reset offsets and overlay state
    mFloatingOffset = QPointF(0,0);
    mOutlineOffset = QPointF(0,0);
    // Clear floating state after commit - selection is now part of base layer
    mHasFloating = false;
    mWasCutFromSource = false;
    mFloatingFromPaste = false;
    mFloatingHybridStrokes.clear();
    mHasFloatingHybrid = false;
    // Increment move count for debugging/optimization
    mMoveCount++;

    // Invalidate composite so CanvasPainter recomputes it on next render (Req 8.1)
    if (stack) stack->invalidateComposite();

    emit selectionChanged();
}

void SelectionManager::applyOutlineOffset()
{
    if (mSelectionPath.isEmpty()) return;
    // Update path and origin by current outline offset
    mSelectionPath.translate(mOutlineOffset);
    mOriginalRect = mSelectionPath.boundingRect();
    mSelectionPolygon = mOriginalRect;
    mFloatingOrigin += mOutlineOffset;
    mOutlineOffset = QPointF(0,0);
    emit selectionChanged();
}

void SelectionManager::applyFloatingOffset()
{
    if (!mHasFloating) return;
    // Bake the floating offset into the origin and selection path without committing to base layer
    // This allows cumulative drags where each drag is relative to the current position
    if (qFuzzyIsNull(mFloatingOffset.x()) && qFuzzyIsNull(mFloatingOffset.y())) return;
    
    mFloatingOrigin += mFloatingOffset;
    if (!mSelectionPath.isEmpty()) {
        mSelectionPath.translate(mFloatingOffset);
        mOriginalRect = mSelectionPath.boundingRect();
        mSelectionPolygon = mOriginalRect;
    }
    if (mHasFloatingHybrid) {
        for (BitmapImage::HybridVectorStroke& stroke : mFloatingHybridStrokes) {
            for (QPointF& p : stroke.points) {
                p += mFloatingOffset;
            }
        }
    }
    // Reset outline offset since we've already translated the selection path
    mOutlineOffset = QPointF(0,0);
    mFloatingOffset = QPointF(0,0);
    emit selectionChanged();
}

bool SelectionManager::isPointInsideMask(const QPointF& canvasPoint) const
{
    if (mSelectionPath.isEmpty()) return false;
    QPainterPath path = selectionPathWithOffsets();
    return path.contains(canvasPoint);
}

bool SelectionManager::isPointOnOutline(const QPointF& canvasPoint) const
{
    if (mSelectionPath.isEmpty()) return false;
    QPainterPath path = selectionPathWithOffsets();
    QPainterPathStroker stroker;
    stroker.setWidth(qMax<qreal>(1.0, selectionTolerance() * 2.0));
    QPainterPath outline = stroker.createStroke(path);
    return outline.contains(canvasPoint);
}

bool SelectionManager::isPointOnBoundingBox(const QPointF& canvasPoint) const
{
    // Use the selection path's bounding rect if available; otherwise fall back to polygon/original rect
    QRectF bbox;
    if (!mSelectionPath.isEmpty()) {
        QPainterPath path = selectionPathWithOffsets();
        bbox = path.boundingRect();
    } else if (!mSelectionPolygon.isEmpty()) {
        bbox = mSelectionPolygon.boundingRect();
    } else if (!mOriginalRect.isNull()) {
        bbox = mOriginalRect;
    } else {
        return false;
    }

    QPainterPath rectPath;
    rectPath.addRect(bbox);
    QPainterPathStroker stroker;
    stroker.setWidth(qMax<qreal>(1.0, selectionTolerance() * 2.0));
    QPainterPath outline = stroker.createStroke(rectPath);
    return outline.contains(canvasPoint);
}

QPainterPath SelectionManager::selectionPathWithOffsets() const
{
    QPainterPath p = mSelectionPath;
    p.translate(mOutlineOffset);
    return p;
}

void SelectionManager::clearAll()
{
    mSelectionPath = QPainterPath();
    mSelectionMask = QImage();
    mFloatingImage = QImage();
    mFloatingOrigin = QPointF(0,0);
    mFloatingOffset = QPointF(0,0);
    mOutlineOffset = QPointF(0,0);
    mHasFloating = false;
    mWasCutFromSource = false;
    mOriginalRect = QRectF();
    mSelectionPolygon = QPolygonF();
    mState = SelectionState::IDLE;
    mOrigin = SelectionOrigin::NONE;
    mFloatingFromPaste = false;
    emit selectionChanged();
}

bool SelectionManager::needsFloatingCommit() const
{
    // Need to commit if we have floating content that was cut from source OR
    // if it originated from a paste operation waiting to be baked into the
    // base bitmap. For paste, we never clear from the source image; we only
    // overlay the floating image at commit time.
    return mHasFloating && (mWasCutFromSource || mFloatingFromPaste);
}


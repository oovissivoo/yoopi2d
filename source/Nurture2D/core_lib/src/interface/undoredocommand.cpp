/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2008-2009 Mj Mendoza IV
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include <QDebug>

#include "undoredocommand.h"
#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h" // Logic needs this
#include "cellbank.h"
#include "keyframe.h"
#include "object.h"

#include "layermanager.h"
#include "selectionmanager.h"

#include "layersound.h"
#include "layervector.h"

UndoRedoCommand::UndoRedoCommand(Editor* editor, QUndoCommand* parent) : QUndoCommand(parent)
{
    qDebug() << "backupElement created";
    mEditor = editor;
    
    // Capture current context
    if (mEditor) {
        mFrameIndex = mEditor->currentFrame();
        if (mEditor->layers() && mEditor->layers()->currentLayer()) {
            mLayerId = mEditor->layers()->currentLayer()->id();
        }
    }
}

void UndoRedoCommand::restoreContext()
{
    if (!mEditor) return;

    // Restore layer for commands that need a valid layer context,
    // but DO NOT move the current frame. Animators expect undo/redo
    // to keep the playhead on the frame they are currently working on.

    Layer* targetLayer = mEditor->layers()->findLayerById(mLayerId);
    if (targetLayer && mEditor->layers()->currentLayer() != targetLayer) {
        mEditor->layers()->setCurrentLayer(targetLayer);
    }
}

UndoRedoCommand::~UndoRedoCommand()
{
}

BitmapReplaceCommand::BitmapReplaceCommand(const BitmapImage* undoBitmap,
                             const int undoLayerId,
                             const QString& description,
                             Editor *editor,
                             QUndoCommand *parent) : UndoRedoCommand(editor, parent)
{
    this->undoBitmap = *undoBitmap;
    this->undoLayerId = undoLayerId;

    // Prefer snapshot from the current layer/current frame (post-modification),
    // with safe fallbacks to covering keyframe or undo snapshot.
    Layer* layer = editor->layers()->currentLayer();
    redoLayerId = layer ? layer->id() : undoLayerId;
    BitmapImage* currentBitmap = nullptr;

    // Attempt to capture Cell ID from the Active KeyFrame at the current frame
    if (layer && layer->type() == Layer::BITMAP) {
        // Try exact frame first, then cover
        KeyFrame* activeKey = layer->getLastKeyFrameAtPosition(editor->currentFrame());
        if (activeKey) {
             BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(activeKey);
             if (bkf) {
                 mCellId = bkf->cellId();
             }
        }

        // Capture redo snapshot from the active inner layer (not the composite)
        LayerBitmap* layerBmp = static_cast<LayerBitmap*>(layer);
        if (mCellId != -1 && layerBmp->cellBank()) {
            currentBitmap = layerBmp->cellBank()->getActiveInnerLayerImage(mCellId);
        }
        if (!currentBitmap) {
            currentBitmap = layerBmp->getBitmapImageAtFrame(editor->currentFrame());
        }
        if (!currentBitmap) {
            if (KeyFrame* cover = layer->getKeyFrameWhichCovers(editor->currentFrame())) {
                BitmapImage* coverBmp = dynamic_cast<BitmapImage*>(cover);
                currentBitmap = coverBmp;
            } else {
                currentBitmap = layerBmp->getLastBitmapImageAtFrame(editor->currentFrame());
            }
        }
    }
    redoBitmap = currentBitmap ? *currentBitmap : *undoBitmap;

    setText(description);
}

void BitmapReplaceCommand::undo()
{
    QUndoCommand::undo();
    Layer* layer = editor()->layers()->findLayerById(undoLayerId);
    if (!layer || layer->type() != Layer::BITMAP) {
        return;
    }

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    if (mCellId != -1 && bitmapLayer->cellBank()) {
        if (!bitmapLayer->cellBank()->getCell(mCellId)) {
            qWarning() << "BitmapReplaceCommand::undo aborted: cell" << mCellId << "no longer exists";
            return;
        }
    }
    
    KeyFrame* targetKey = nullptr;
    
    // Robust Lookup: Find KeyFrame by Cell ID
    if (mCellId != -1) {
        layer->foreachKeyFrame([&](KeyFrame* kf){
            if (targetKey) return; // Found
            BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
            if (bkf && bkf->cellId() == mCellId) {
                targetKey = kf;
            }
        });
    }
    
    // Fallback: Use current frame mapping
    if (!targetKey) {
        targetKey = layer->getLastKeyFrameAtPosition(editor()->currentFrame());
    }
    
    if (!targetKey) {
        qDebug() << "[BitmapUndo] No target keyframe found for cell" << mCellId;
        return;
    }
    
    // Update undo bitmap position to match found keyframe (in case it moved)
    undoBitmap.setPos(targetKey->pos());
    static_cast<LayerBitmap*>(layer)->replaceKeyFrame(&undoBitmap);
    
    // Always repaint to show result of undo immediately
    emit editor()->frameModified(editor()->currentFrame());
    editor()->updateFrame();
}

void BitmapReplaceCommand::redo()
{
    QUndoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    Layer* layer = editor()->layers()->findLayerById(redoLayerId);
    if (!layer || layer->type() != Layer::BITMAP) {
        qWarning() << "BitmapReplaceCommand::redo aborted: invalid or non-bitmap layer";
        return;
    }

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    if (mCellId != -1 && bitmapLayer->cellBank()) {
        if (!bitmapLayer->cellBank()->getCell(mCellId)) {
            qWarning() << "BitmapReplaceCommand::redo aborted: cell" << mCellId << "no longer exists";
            return;
        }
    }
    
    KeyFrame* kf = nullptr;
    if (mCellId != -1) {
        layer->foreachKeyFrame([&](KeyFrame* keyFrame){ // Renamed local var to avoid shadowing if any
            if (kf) return;
            BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(keyFrame);
            if (bkf && bkf->cellId() == mCellId) {
                kf = bkf;
            }
        });
    }
    if (!kf) kf = layer->getLastKeyFrameAtPosition(editor()->currentFrame());
    if (!kf) return;
    
    redoBitmap.setPos(kf->pos());
    static_cast<LayerBitmap*>(layer)->replaceKeyFrame(&redoBitmap);
    
    emit editor()->frameModified(editor()->currentFrame());
    editor()->updateFrame();
}

VectorReplaceCommand::VectorReplaceCommand(const VectorImage* undoVector,
                                   const int undoLayerId,
                                   const QString& description,
                                   Editor* editor,
                                   QUndoCommand* parent) : UndoRedoCommand(editor, parent)
{
    this->undoVector = *undoVector;
    this->undoLayerId = undoLayerId;

    // Prefer snapshot from current layer/current frame (post-modification),
    // fallback to covering keyframe or undo snapshot.
    Layer* layer = editor->layers()->currentLayer();
    redoLayerId = layer ? layer->id() : undoLayerId;
    VectorImage* currentVector = nullptr;
    if (layer && layer->type() == Layer::VECTOR) {
        currentVector = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(editor->currentFrame());
        if (!currentVector) {
            if (KeyFrame* cover = layer->getKeyFrameWhichCovers(editor->currentFrame())) {
                VectorImage* coverVec = dynamic_cast<VectorImage*>(cover);
                Q_ASSERT_X(coverVec != nullptr, "VectorReplaceCommand", "Covering keyframe is not a VectorImage for a VECTOR layer");
                currentVector = coverVec;
            }
        }
    }
    redoVector = currentVector ? *currentVector : *undoVector;

    setText(description);
}
void VectorReplaceCommand::undo()
{
    QUndoCommand::undo();

    Layer* layer = editor()->layers()->findLayerById(undoLayerId);
    if (!layer || layer->type() != Layer::VECTOR) return;
    VectorImage* currentVector = nullptr;
    if (layer->type() == Layer::VECTOR) {
        currentVector = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(editor()->currentFrame());
        if (!currentVector) {
            if (KeyFrame* cover = layer->getKeyFrameWhichCovers(editor()->currentFrame())) {
                currentVector = static_cast<VectorImage*>(cover);
            }
        }
    }
    if (!currentVector) {
        currentVector = &undoVector;
    }
    static_cast<LayerVector*>(layer)->replaceKeyFrame(currentVector);
    // Keep user at current visible frame (may be a hold). Trigger repaint.
    emit editor()->frameModified(editor()->currentFrame());
    editor()->updateFrame();
}

void VectorReplaceCommand::redo()
{
    QUndoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    Layer* layer = editor()->layers()->findLayerById(redoLayerId);
    if (!layer || layer->type() != Layer::VECTOR) return;
    VectorImage* currentVector = nullptr;
    if (layer->type() == Layer::VECTOR) {
        currentVector = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(editor()->currentFrame());
        if (!currentVector) {
            if (KeyFrame* cover = layer->getKeyFrameWhichCovers(editor()->currentFrame())) {
                currentVector = static_cast<VectorImage*>(cover);
            }
        }
    }
    if (!currentVector) {
        currentVector = &redoVector;
    }
    static_cast<LayerVector*>(layer)->replaceKeyFrame(currentVector);

    emit editor()->frameModified(editor()->currentFrame());
    editor()->updateFrame();
}

TransformCommand::TransformCommand(const QRectF& undoSelectionRect,
                                   const QPointF& undoTranslation,
                                   const qreal undoRotationAngle,
                                   const qreal undoScaleX,
                                   const qreal undoScaleY,
                                   const QPointF& undoTransformAnchor,
                                   const bool roundPixels,
                                   const QString& description,
                                   Editor* editor,
                                   QUndoCommand *parent) : UndoRedoCommand(editor, parent)
{
    this->roundPixels = roundPixels;

    this->undoSelectionRect = undoSelectionRect;
    this->undoAnchor = undoTransformAnchor;
    this->undoTranslation = undoTranslation;
    this->undoRotationAngle = undoRotationAngle;
    this->undoScaleX = undoScaleX;
    this->undoScaleY = undoScaleY;

    auto selectMan = editor->select();
    redoSelectionRect = selectMan->mySelectionRect();
    redoAnchor = selectMan->currentTransformAnchor();
    redoTranslation = selectMan->myTranslation();
    redoRotationAngle = selectMan->myRotation();
    redoScaleX = selectMan->myScaleX();
    redoScaleY = selectMan->myScaleY();

    setText(description);
}

void TransformCommand::undo()
{
    apply(undoSelectionRect,
          undoTranslation,
          undoRotationAngle,
          undoScaleX,
          undoScaleY,
          undoAnchor,
          roundPixels);
}

void TransformCommand::redo()
{
    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(redoSelectionRect,
          redoTranslation,
          redoRotationAngle,
          redoScaleX,
          redoScaleY,
          redoAnchor,
          roundPixels);
}

void TransformCommand::apply(const QRectF& selectionRect,
                             const QPointF& translation,
                             const qreal rotationAngle,
                             const qreal scaleX,
                             const qreal scaleY,
                             const QPointF& selectionAnchor,
                             const bool roundPixels)
{
    auto selectMan = editor()->select();
    if (!selectMan) {
        qWarning() << "TransformCommand::apply aborted: selection manager is null";
        return;
    }
    selectMan->setSelection(selectionRect, roundPixels);
    selectMan->setTransformAnchor(selectionAnchor);
    selectMan->setTranslation(translation);
    selectMan->setRotation(rotationAngle);
    selectMan->setScale(scaleX, scaleY);

    selectMan->calculateSelectionTransformation();
}

KeyFrameRemoveCommand::KeyFrameRemoveCommand(KeyFrame* removedKeyFrame,
                                             const int layerId,
                                             const QString& description,
                                             Editor* editor,
                                             QUndoCommand* parent)
    : UndoRedoCommand(editor, parent)
{
    mLayerId = layerId;
    mFramePosition = removedKeyFrame->pos();
    mRemovedKeyFrame = std::unique_ptr<KeyFrame>(removedKeyFrame->clone());
    auto* layer = dynamic_cast<LayerBitmap*>(editor->layers()->findLayerById(layerId));
    if (layer && layer->cellBank()) {
        mOriginalCellOrder = layer->cellBank()->getAllCellIds();
        for (int id : mOriginalCellOrder) mOriginalCellNames[id] = layer->cellBank()->getCellName(id);
    }
    setText(description);
}

KeyFrameRemoveCommand::~KeyFrameRemoveCommand()
{
}

void KeyFrameRemoveCommand::undo()
{
    QUndoCommand::undo();
    
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) {
        qWarning() << "KeyFrameRemoveCommand::undo aborted: layer is null";
        return;
    }

    if (layer->type() == Layer::BITMAP) {
        // Do not resurrect exposures for cells that have been permanently deleted.
        auto* bitmapKF = dynamic_cast<BitmapKeyFrame*>(mRemovedKeyFrame.get());
        if (bitmapKF) {
            auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
            if (bitmapLayer->cellBank() && !bitmapLayer->cellBank()->getCell(bitmapKF->cellId())) {
                qWarning() << "KeyFrameRemoveCommand::undo skipped: cell" << bitmapKF->cellId()
                           << "was permanently deleted";
                return;
            }
        }
    }
    
    // Re-add the keyframe that was removed (only if its cell is still valid)
    layer->addKeyFrame(mFramePosition, mRemovedKeyFrame->clone());
    if (auto* bitmap = dynamic_cast<LayerBitmap*>(layer)) {
        bitmap->cellBank()->setCellOrder(mOriginalCellOrder);
        for (auto it = mOriginalCellNames.cbegin(); it != mOriginalCellNames.cend(); ++it)
            bitmap->cellBank()->setCellName(it.key(), it.value());
        bitmap->renumberCellsByTimeline();
    }
    
    editor()->layers()->notifyAnimationLengthChanged();
    editor()->layers()->notifyLayerChanged(layer);
    emit editor()->updateTimeLine();
    emit editor()->framesModified();
}

void KeyFrameRemoveCommand::redo()
{
    QUndoCommand::redo();
    
    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }
    
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) {
        qWarning() << "KeyFrameRemoveCommand::redo aborted: layer is null";
        return;
    }
    
    // Remove the keyframe again
    layer->removeKeyFrame(mFramePosition);
    if (auto* bitmap = dynamic_cast<LayerBitmap*>(layer)) bitmap->renumberCellsByTimeline();
    
    editor()->layers()->notifyAnimationLengthChanged();
    editor()->layers()->notifyLayerChanged(layer);
    emit editor()->updateTimeLine();
    editor()->scrubTo(mFramePosition);
    emit editor()->framesModified();
}

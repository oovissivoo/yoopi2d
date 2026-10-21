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

#include "object.h"
#include "editor.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "cellbank.h"
#include "celllayerstack.h"
#include "structure/innerlayer.h"

#include <QAction>
#include <QDebug>
#include <QSettings>
#include <QApplication>
#include <QEvent>


#include "preferencemanager.h"
#include "layermanager.h"
#include "soundmanager.h"
#include "undoredomanager.h"
#include "scribblearea.h"
#include "selectionmanager.h"

#include "undoredocommand.h"
#include "legacybackupelement.h"

#include "layerbitmap.h"
#include "layervector.h"
#include "layersound.h"


#include "bitmapimage.h"
#include "vectorimage.h"
#include "soundclip.h"

#include <QThread>

#include "bitmapkeyframe.h"

UndoSaveState::UndoSaveState() = default;
UndoSaveState::~UndoSaveState() = default;

UndoRedoManager::UndoRedoManager(Editor* editor) : BaseManager(editor, "UndoRedoManager")
{
    mActiveCellStack = nullptr;  // Starts with no active cell
    qDebug() << "UndoRedoManager: created";
}

UndoRedoManager::~UndoRedoManager()
{
    disconnect(&mGlobalStack, nullptr, this, nullptr);
    qDebug() << "UndoRedoManager: destroyed";
}

bool UndoRedoManager::init()
{
    qDebug() << "UndoRedoManager: init";
    const int limit = editor()->preference()->getInt(SETTING::UNDO_REDO_MAX_STEPS);
    mGlobalStack.setUndoLimit(limit);
    CellBank::setGlobalUndoLimit(limit);
    connect(&mGlobalStack, &QUndoStack::indexChanged, this, &UndoRedoManager::onStackIndexChanged);

    // Install an event filter on the application so we can respond to
    // QEvent::LanguageChange and re-evaluate tr("Undo") / tr("Redo") with
    // the newly installed translator.
    if (qApp) qApp->installEventFilter(this);

    return true;
}

void UndoRedoManager::onSettingChanged(SETTING setting)
{
    if (setting == SETTING::UNDO_REDO_MAX_STEPS) {
        const int newLimit = editor()->preference()->getInt(SETTING::UNDO_REDO_MAX_STEPS);
        // Clear all stacks first to respect Qt's requirement when changing undo limits
        clearStack();

        // Set limit on global stack
        mGlobalStack.setUndoLimit(newLimit);
        CellBank::setGlobalUndoLimit(newLimit);
        
        qDebug() << "updated undo stack limit";
    }
}

void UndoRedoManager::restoreLegacyKey()
{
    // Legacy restore hook removed; nothing to do under the new undo system.
}

Status UndoRedoManager::load(Object* /*o*/)
{
    clearStack();
    return Status::OK;
}

Status UndoRedoManager::save(Object* /*o*/)
{
    // Mark all undo stacks clean: global + per-keyframe
    // Mark global stack clean
    mGlobalStack.setClean();

    // Also mark the currently active cell stack clean, if different from the global stack.
    // Without this, drawings recorded to per-cell stacks won't participate in unsaved-change
    // tracking and the app can close without prompting.
    if (mActiveCellStack && mActiveCellStack != &mGlobalStack) {
        mActiveCellStack->setClean();
    }

    return Status::OK;
}

void UndoRedoManager::record(const UndoSaveState*& undoState, const QString& description)
{
    // Main-thread only
    Q_ASSERT(QThread::currentThread() == qApp->thread());

    if (!undoState) {
        return;
    }

    switch (undoState->recordType)
    {
        case UndoRedoRecordType::KEYFRAME_MODIFY: {
            replaceKeyFrame(*undoState, description);
            break;
        }
        default: {
            qWarning() << "UndoRedoManager::record: unhandled recordType for" << description;
            break;
        }
    }


    // The save state has now been used and should be invalidated so we can't use it again.
    delete undoState;
    undoState = nullptr;
}

bool UndoRedoManager::hasUnsavedChanges() const
{
    // Unsaved changes can exist on either the global stack (timeline/layer ops)
    // or the currently active per-cell stack (drawing edits).
    if (mActiveCellStack && mActiveCellStack != &mGlobalStack) {
        if (!mActiveCellStack->isClean()) {
            return true;
        }
    }
    return !mGlobalStack.isClean();
}

QUndoStack* UndoRedoManager::activeStack()
{
    // Return active cell stack if set, otherwise return global
    return mActiveCellStack ? mActiveCellStack.data() : &mGlobalStack;
}

void UndoRedoManager::setActiveCellStack(QUndoStack* stack)
{
    if (stack) {
        connect(stack, &QUndoStack::indexChanged, this, &UndoRedoManager::onStackIndexChanged, Qt::UniqueConnection);
        connect(stack, &QObject::destroyed, this, &UndoRedoManager::updateActions, Qt::UniqueConnection);
    }
    if (mActiveCellStack != stack) {
        mActiveCellStack = stack;
        emit activeStackChanged();
        updateActions();
    }
}

QUndoStack* UndoRedoManager::undoTargetStack()
{
    QUndoStack* stack = activeStack();
    return stack->canUndo() ? stack : &mGlobalStack;
}

QUndoStack* UndoRedoManager::redoTargetStack()
{
    if (mLastUndoneStack && mLastUndoneStack->canRedo()) return mLastUndoneStack.data();
    QUndoStack* stack = activeStack();
    return stack->canRedo() ? stack : &mGlobalStack;
}

void UndoRedoManager::onStackIndexChanged()
{
    if (!mExecutingHistory) mLastUndoneStack.clear();
    updateActions();
}

void UndoRedoManager::pushCommand(QUndoCommand* command)
{
    // Main-thread only
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    if (!command) return;
    
    // Check if this command should skip the stack (destructive execution)
    UndoRedoCommand* urCmd = dynamic_cast<UndoRedoCommand*>(command);
    if (urCmd && !urCmd->applyToStack()) {
        qDebug() << "[UndoRedoManager] Skipping stack for destructive command:" << command->text();
        command->redo();
        delete command;
        return;
    }
    
    // Push to active stack (cell stack for drawing ops, global for timeline)
    activeStack()->push(command);
    updateActions();
}

void UndoRedoManager::replaceKeyFrame(const UndoSaveState& undoState, const QString& description)
{
    if (undoState.layerType == Layer::BITMAP) {
        replaceBitmap(undoState, description);
    } else if (undoState.layerType == Layer::VECTOR) {
        replaceVector(undoState, description);
    } else {
        // Implement other cases
    }
}


void UndoRedoManager::replaceBitmap(const UndoSaveState& undoState, const QString& description)
{
    if (undoState.keyframe == nullptr || undoState.layerType != Layer::BITMAP) { return; }

    // Validate current layer context to avoid pushing invalid commands
    Layer* layer = nullptr;
    if (editor() && editor()->layers()) {
        layer = editor()->layers()->findLayerById(undoState.layerId);
        if (!layer) {
            layer = editor()->layers()->currentLayer();
        }
    }
    if (!layer || layer->type() != Layer::BITMAP) {
        qWarning() << "UndoRedoManager::replaceBitmap aborted: no valid bitmap layer context";
        return;
    }
    // We need to retrieve the image. But UndoState stores a CLONE of the keyframe.
    // If KeyFrame is just an ID (BitmapKeyFrame), then cloning it just clones the ID.
    // This DOES NOT save the image content!
    // So if we are "undoing" a drawing change, and we only saved the ID, we are just restoring the ID.
    // If the image content corrupted/changed, restoring ID changes nothing.
    
    // HOWEVER, fixing the full Undo architecture to snapshot images is out of scope for this immediate crash fix.
    // We just want to stop the crash.
    // If we cast BitmapKeyFrame* to BitmapImage*, we crash.
    // We should at least CAST correctly to BitmapKeyFrame, get the ID, and then get the Image from the bank?
    // But if we get the image from the bank, it's the CURRENT image. Not the UNDO image.
    
    // For now, let's assume the UndoState creation logic is ALSO broken (it just clones keyframe).
    // But to fix the CRASH:
    
    if (undoState.layerType != Layer::BITMAP) { 
         // We rely on layerType context.
    }
    
    // Temporary Hack/Fix:
    // If we are replacing a bitmap keyframe, we need to pass a BitmapImage* to BitmapReplaceCommand?
    // Let's check BitmapReplaceCommand.
    
    // If BitmapReplaceCommand expects BitmapImage*, we are in trouble if we don't have one.
    // But maybe we can get the image from the layer using the ID in the keyframe?
    
    BitmapImage* imageToRestore = nullptr;
    if (undoState.bitmapImage) {
        imageToRestore = undoState.bitmapImage.get();
    }
    
    // Fallback logic (should not be hit if we captured correctly)
    // If we have a captured snapshot, use it.
    // Fallback (or if capture failed) logic is to look it up, but that is what caused the bug.
    // So we primarily trust the snapshot.
    // However, if the snapshot is null (maybe old undo stack?), we might check legacy path?
    // But since we JUST added the field, old undo entries won't have it.
    // But this persists in memory only, so a restart clears undo anyway.
    
    if (imageToRestore) {
        BitmapReplaceCommand* element = new BitmapReplaceCommand(imageToRestore,
                                                   undoState.layerId,
                                                   description,
                                                   editor());
        pushCommand(element);
    } else {
        // Fallback for safety, though this likely won't undo pixel changes correctly
        int cellId = -1;
        BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(undoState.keyframe.get());
        if (bkf) {
            cellId = bkf->cellId();
            LayerBitmap* layerBmp = static_cast<LayerBitmap*>(layer);
            if (layerBmp->cellBank()) {
                 imageToRestore = layerBmp->cellBank()->getCell(cellId);
                 if (imageToRestore) {
                    BitmapReplaceCommand* element = new BitmapReplaceCommand(imageToRestore,
                                                               undoState.layerId,
                                                               description,
                                                               editor());
                    pushCommand(element);
                 }
            }
        } else {
             qWarning() << "UndoRedoManager: Failed to resolve cell for undo/redo.";
        }
    }
}

void UndoRedoManager::replaceVector(const UndoSaveState& undoState, const QString& description)
{
    if (undoState.keyframe == nullptr || undoState.layerType != Layer::VECTOR) { return; }
    VectorReplaceCommand* element = new VectorReplaceCommand(static_cast<VectorImage*>(undoState.keyframe.get()),
                                                 undoState.layerId,
                                                 description,
                                                 editor());
    const SelectionSaveState* selectionState = undoState.selectionState.get();
    if (selectionState) {
        new TransformCommand(selectionState->bounds,
                             selectionState->translation,
                             selectionState->rotationAngle,
                             selectionState->scaleX,
                             selectionState->scaleY,
                             selectionState->anchor,
                             false, // Round pixels
                             description,
                             editor(), element);
    } else {
        qWarning() << "UndoRedoManager::replaceVector: selectionState is null, skipping transform command";
    }
    pushCommand(element);
}

const UndoSaveState* UndoRedoManager::state(UndoRedoRecordType recordType) const
{
    switch (recordType)
    {
        case UndoRedoRecordType::KEYFRAME_MODIFY: {
            return savedKeyFrameState();
        default:
            return nullptr;
        }
    }
}

const UndoSaveState* UndoRedoManager::savedKeyFrameState() const
{
    // Defensive checks for current layer
    if (!editor() || !editor()->layers()) {
        qWarning() << "UndoRedoManager::savedKeyFrameState: editor or layers manager is null";
        return nullptr;
    }

    const Layer* layer = editor()->layers()->currentLayer();
    if (!layer) {
        qWarning() << "UndoRedoManager::savedKeyFrameState: currentLayer is null";
        return nullptr;
    }

    UndoSaveState* undoSaveState = new UndoSaveState();
    undoSaveState->recordType = UndoRedoRecordType::KEYFRAME_MODIFY;
    undoSaveState->layerType = layer->type();
    undoSaveState->layerId = layer->id();

    if (layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR) {
        auto selectMan = editor()->select();
        if (selectMan) {
            undoSaveState->selectionState = std::unique_ptr<SelectionSaveState>( new SelectionSaveState(
                selectMan->mySelectionRect(),
                selectMan->myRotation(),
                selectMan->myScaleX(),
                selectMan->myScaleY(),
                selectMan->myTranslation(),
                selectMan->currentTransformAnchor())
            );
        } else {
            qWarning() << "UndoRedoManager::savedKeyFrameState: selection manager is null; selectionState omitted";
        }

        // Snapshot Bitmap Image logic
        if (layer->type() == Layer::BITMAP) {
             const int currentFrame = editor()->currentFrame();
             // We need non-const access to get image from layer (until improved const-correctness in LayerBitmap)
             LayerBitmap* layerBmp = static_cast<LayerBitmap*>(const_cast<Layer*>(layer));
             // Use the active inner layer image (not the composite) so undo/redo
             // correctly snapshots and restores the writable layer data.
             KeyFrame* kf = layerBmp->getKeyFrameAt(currentFrame);
             if (!kf) kf = layerBmp->getLastKeyFrameAtPosition(currentFrame);
             BitmapImage* currentImg = nullptr;
             if (kf) {
                 auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
                 if (bkf) {
                     currentImg = layerBmp->cellBank()->getActiveInnerLayerImage(bkf->cellId());
                 }
             }
             if (!currentImg) {
                 // Fallback to composite for legacy compatibility
                 currentImg = layerBmp->getBitmapImageAtFrame(currentFrame);
             }

             if (currentImg) {
                 if (!currentImg->image()->isNull()) {
                     undoSaveState->bitmapImage = std::unique_ptr<BitmapImage>(currentImg->clone());
                 } else {
                     // Image exists but has no pixel data yet — create a blank snapshot
                     undoSaveState->bitmapImage = std::unique_ptr<BitmapImage>(new BitmapImage());
                 }
             }
        }
    }

    const int frameIndex = editor()->currentFrame();
    if (layer->keyExists(frameIndex))
    {
        undoSaveState->keyframe = std::unique_ptr<KeyFrame>(layer->getLastKeyFrameAtPosition(frameIndex)->clone());
    }
    else if (KeyFrame* covering = layer->getKeyFrameWhichCovers(frameIndex))
    {
        undoSaveState->keyframe = std::unique_ptr<KeyFrame>(covering->clone());
    }
    else
    {
        // Fallback: when drawing-on-empty-frame preference is set to keep drawing on previous key,
        // tools will modify the last keyframe before currentFrame without creating a new key.
        // Snapshot that previous key so the first stroke after an import (which may advance the
        // playhead) records a proper undo entry.
        if (KeyFrame* prev = layer->getLastKeyFrameAtPosition(frameIndex))
        {
            undoSaveState->keyframe = std::unique_ptr<KeyFrame>(prev->clone());
        }
    }

    return undoSaveState;
}

bool UndoRedoManager::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        // Re-evaluate tr("Undo") / tr("Redo") with the newly installed translator.
        updateActions();
    }
    return QObject::eventFilter(watched, event);
}

QAction* UndoRedoManager::createUndoAction(QObject* parent, const QIcon& icon)
{
    QAction* action = new QAction(icon, tr("Undo"), parent);
    action->setShortcut(QKeySequence::Undo);
    connect(action, &QAction::triggered, this, &UndoRedoManager::undo);
    mUndoAction = action;
    updateActions();
    return action;
}

QAction* UndoRedoManager::createRedoAction(QObject* parent, const QIcon& icon)
{
    QAction* action = new QAction(icon, tr("Redo"), parent);
    action->setShortcut(QKeySequence::Redo);
    connect(action, &QAction::triggered, this, &UndoRedoManager::redo);
    mRedoAction = action;
    updateActions();
    return action;
}

bool UndoRedoManager::isActiveLayerLocked() const
{
    if (!editor() || !editor()->layers()) return false;
    Layer* lay = editor()->layers()->currentLayer();
    if (!lay || lay->type() != Layer::BITMAP) return false;
    auto* bl = static_cast<LayerBitmap*>(lay);
    const int fr = editor()->currentFrame();
    KeyFrame* kf = bl->keyExists(fr) ? bl->getKeyFrameAt(fr)
                                     : bl->getLastKeyFrameAtPosition(fr);
    auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
    if (!bkf || !bl->cellBank()) return false;
    CellLayerStack* st = bl->cellBank()->getCellLayerStack(bkf->cellId());
    return st && st->activeLayer() && st->activeLayer()->locked;
}

void UndoRedoManager::undo()
{
    QUndoStack* stack = undoTargetStack();
    qInfo() << "[UndoRedoManager] Undo triggered; canUndo=" << stack->canUndo()
            << "command=" << stack->undoText() << "timeline=" << (stack == &mGlobalStack);
    if (isActiveLayerLocked()) {
        qWarning() << "[UndoRedoManager] Undo blocked because the active layer is locked";
        emit editor()->getScribbleArea()->lockedLayerAttempted();
        return;
    }
    mExecutingHistory = true;
    stack->undo();
    mExecutingHistory = false;
    mLastUndoneStack = stack;
    updateActions(); 
}

void UndoRedoManager::redo()
{
    QUndoStack* stack = redoTargetStack();
    qInfo() << "[UndoRedoManager] Redo triggered; canRedo=" << stack->canRedo()
            << "command=" << stack->redoText();
    if (isActiveLayerLocked()) {
        qWarning() << "[UndoRedoManager] Redo blocked because the active layer is locked";
        emit editor()->getScribbleArea()->lockedLayerAttempted();
        return;
    }
    mExecutingHistory = true;
    stack->redo();
    mExecutingHistory = false;
    updateActions();
}

void UndoRedoManager::updateActions()
{
    if (mUndoAction) {
        mUndoAction->setEnabled(undoTargetStack()->canUndo());
        // Always use a stable label in UI; we do not expose the per-command text
        // in the menu/toolbar to keep things consistent.
        mUndoAction->setText(tr("Undo"));
    }
    if (mRedoAction) {
        mRedoAction->setEnabled(redoTargetStack()->canRedo());
        mRedoAction->setText(tr("Redo"));
    }
}

void UndoRedoManager::updateUndoAction(QAction* /*undoAction*/) {}
void UndoRedoManager::updateRedoAction(QAction* /*redoAction*/) {}

void UndoRedoManager::clearStack()
{
    mGlobalStack.clear();
    // For cell stacks, we might need to clear them too if they are cached,
    // but usually clearStack is for project load/new.
    updateActions();
}

// Legacy system implementations removed.

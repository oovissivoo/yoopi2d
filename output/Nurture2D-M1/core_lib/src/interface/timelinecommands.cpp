#include "timelinecommands.h"
#include "editor.h"
#include "layer.h"
#include "layermanager.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "cellbank.h"
#include "object.h"
#include <QDebug>

// ===========================================
// AddKeyFrameCommand
// ===========================================

AddKeyFrameCommand::AddKeyFrameCommand(int frameIndex, int layerId, Editor* editor)
    : UndoRedoCommand(editor)
{
    mFrameIndex = frameIndex;
    mLayerId = layerId;
    setText(QObject::tr("Add Keyframe"));
    setApplyToStack(false);
}

AddKeyFrameCommand::~AddKeyFrameCommand()
{
}

void AddKeyFrameCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) return;

    // Undo Add = Remove
    layer->removeKeyFrame(mFrameIndex);
    
    // Notify
    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mFrameIndex);
    editor()->updateFrame();
    editor()->updateUndoContext();
}

void AddKeyFrameCommand::redo()
{
    // Ignore first redo if the command was just pushed (because action already happened)
    // BUT wait: Editor::addNewKey() performs the action. 
    // Usually QUndoStack::push() calls redo() immediately.
    // So we should let redo() do the work?
    // User pattern: Editor::addNewKey() calls Layer::addNewKeyFrameAt().
    // If we change Editor::addNewKey() to push command, then pushing command will trigger redo(), which will call Layer::addNewKeyFrameAt().
    // This is the clean way!
    
    // However, we need to handle "isFirstRedo" carefully if we want to support both modes or migration.
    // The clean way is: Editor::addNewKey() constructs Command and pushes it. 
    // Command::redo() calls layer->addNewKeyFrameAt().
    // So we DON'T check isFirstRedo() logic here unless we are doing "post-facto" recording.
    // Let's assume standard Command pattern where Redo does the work.
    
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) return;

    if (!layer->keyExists(mFrameIndex)) {
        layer->addNewKeyFrameAt(mFrameIndex);
    }
    
    // Notify
    editor()->updateTimeLine();
    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mFrameIndex);
    editor()->updateFrame();
    editor()->updateUndoContext();
}

// ===========================================
// RemoveKeyFrameCommand
// ===========================================

RemoveKeyFrameCommand::RemoveKeyFrameCommand(int frameIndex, int layerId, Editor* editor)
    : UndoRedoCommand(editor)
{
    mFrameIndex = frameIndex;
    mLayerId = layerId;
    setText(QObject::tr("Remove Keyframe"));
    
    // Backup data immediately
    Layer* layer = editor->layers()->findLayerById(mLayerId);
    if (layer) {
        if (KeyFrame* kf = layer->getKeyFrameAt(mFrameIndex)) {
            mBackupKeyFrame = kf->clone();
        }
    }
    setApplyToStack(false);
}

RemoveKeyFrameCommand::~RemoveKeyFrameCommand()
{
    if (mBackupKeyFrame) {
        delete mBackupKeyFrame; 
        // Note: Logic needed to prevent double delete if ownership transferred back to layer?
        // Standard pattern: 
        // - In Undo (restore): Layer takes ownership? Or we clone backup?
        // - In Destructor: If we still hold it, delete it.
        // If Layer takes ownership, we should release our pointer or clone it.
        // Cloning is safer but heavier.
        // Let's rely on cloning for restoration to avoid ownership soup.
    }
}

void RemoveKeyFrameCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer || !mBackupKeyFrame) return;

    // Undo Remove = Add (Restore)
    KeyFrame* restored = mBackupKeyFrame->clone();
    layer->addKeyFrame(mFrameIndex, restored);
    
    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mFrameIndex);
    editor()->updateFrame();
}

void RemoveKeyFrameCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) return;

    // Redo Remove = Remove
    layer->removeKeyFrame(mFrameIndex);
    
    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mFrameIndex);
    editor()->updateFrame();
}

// ===========================================
// DuplicateKeyFrameCommand
// ===========================================

DuplicateKeyFrameCommand::DuplicateKeyFrameCommand(int srcFrame, int dstFrame, int layerId, Editor* editor)
    : UndoRedoCommand(editor), mSrcFrame(srcFrame), mDstFrame(dstFrame)
{
    mLayerId = layerId;
    mFrameIndex = dstFrame; // Context for this command is the *destination* frame
    setText(QObject::tr("Duplicate Keyframe"));
    
    // Backup if destination exists
    Layer* layer = editor->layers()->findLayerById(mLayerId);
    if (layer) {
        if (KeyFrame* kf = layer->getKeyFrameAt(mDstFrame)) {
            mBackupKey = kf->clone();
        }
    }
    setApplyToStack(false);
}

void DuplicateKeyFrameCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) return;

    // Undo Duplicate:
    // 1. Remove the duplicated key at dst
    layer->removeKeyFrame(mDstFrame);
    
    // 2. If we overwrote something, restore it
    if (mBackupKey) {
        // Clone from backup to put into layer
        layer->addKeyFrame(mDstFrame, mBackupKey->clone());
    }

    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mDstFrame);
    editor()->updateFrame();
    editor()->updateUndoContext();
}

void DuplicateKeyFrameCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) return;

    // Redo Duplicate:
    // 1. Get Source
    KeyFrame* srcKey = layer->getKeyFrameAt(mSrcFrame);
    if (!srcKey) return; 

    // 2. Remove Dest if exists (backup was taken in constructor/logic)
    if (layer->keyExists(mDstFrame)) {
        layer->removeKeyFrame(mDstFrame);
    }
    
    // 3. Clone Src -> Dst
    KeyFrame* dupKey = srcKey->clone();
    layer->addKeyFrame(mDstFrame, dupKey);

    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mDstFrame);
    editor()->updateFrame();
}

// ===========================================
// MakeUniqueDrawingCommand
// ===========================================

MakeUniqueDrawingCommand::MakeUniqueDrawingCommand(int frameIndex, int layerId, Editor* editor)
    : UndoRedoCommand(editor)
{
    mFrameIndex = frameIndex;
    mLayerId = layerId;
    setText(QObject::tr("Make Drawing Unique"));
    setApplyToStack(false);
}

MakeUniqueDrawingCommand::~MakeUniqueDrawingCommand()
{
    // mNewCellId is owned by the CellBank; we don't delete it here.
    // On undo the new cell is removed from the bank; on redo it's re-added.
    // The bank owns all cell memory.
}

void MakeUniqueDrawingCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer || layer->type() != Layer::BITMAP) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    CellBank* bank = bitmapLayer->cellBank();
    if (!bank) return;

    KeyFrame* kf = layer->getKeyFrameAt(mFrameIndex);
    if (!kf) return;
    BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
    if (!bkf) return;

    // Restore the original shared cell reference
    bank->removeInstance(mNewCellId, mLayerId, mFrameIndex);
    bkf->setCellId(mOldCellId);
    bank->addInstance(mOldCellId, mLayerId, mFrameIndex);

    // Remove the duplicate cell from the bank
    bank->removeCell(mNewCellId);

    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mFrameIndex);
    editor()->updateFrame();
}

void MakeUniqueDrawingCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer || layer->type() != Layer::BITMAP) return;

    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    CellBank* bank = bitmapLayer->cellBank();
    if (!bank) return;

    KeyFrame* kf = layer->getKeyFrameAt(mFrameIndex);
    if (!kf) return;
    BitmapKeyFrame* bkf = dynamic_cast<BitmapKeyFrame*>(kf);
    if (!bkf) return;

    mOldCellId = bkf->cellId();

    // Deep-copy the cell to get a brand-new independent drawing
    mNewCellId = bank->duplicateCell(mOldCellId);
    if (mNewCellId < 0) return;

    // Give the new cell a name derived from the original
    const QString baseName = bank->getCellName(mOldCellId);
    bank->setCellName(mNewCellId, bank->generateNextName(baseName));

    // Swap the keyframe's cell reference to the new unique cell
    bank->removeInstance(mOldCellId, mLayerId, mFrameIndex);
    bkf->setCellId(mNewCellId);
    bank->addInstance(mNewCellId, mLayerId, mFrameIndex);

    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mFrameIndex);
    editor()->updateFrame();
}

// ===========================================
// MoveKeyFrameCommand
// ===========================================

MoveKeyFrameCommand::MoveKeyFrameCommand(int srcFrame, int dstFrame, int layerId, Editor* editor)
    : UndoRedoCommand(editor), mSrcFrame(srcFrame), mDstFrame(dstFrame)
{
    mLayerId = layerId;
    mFrameIndex = dstFrame; // Ending context
    setText(QObject::tr("Move Keyframe"));
    setApplyToStack(false);
}

void MoveKeyFrameCommand::undo()
{
    // Undo Move: Move back from Dest to Src
    // Offset = dst - src
    // Reverse Offset = src - dst = -(dst - src)
    int offset = mDstFrame - mSrcFrame;

    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) return;
    
    // Track cell instance move for bitmap layers
    if (layer->type() == Layer::BITMAP) {
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        KeyFrame* kf = layer->getKeyFrameAt(mDstFrame);
        if (kf) {
            BitmapKeyFrame* bitmapKF = dynamic_cast<BitmapKeyFrame*>(kf);
            if (bitmapKF && bitmapKF->cellId() != -1 && bitmapLayer->cellBank()) {
                bitmapLayer->cellBank()->moveInstance(bitmapKF->cellId(), mLayerId, mDstFrame, mSrcFrame);
            }
        }
    }
    
    layer->moveKeyFrame(mDstFrame, -offset);

    // Force a full update because the keyframe structure changed
    editor()->object()->updateActiveFrames(mSrcFrame);

    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mSrcFrame);
    editor()->updateFrame();
    editor()->updateUndoContext();
}

void MoveKeyFrameCommand::redo()
{
    int offset = mDstFrame - mSrcFrame;

    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) return;

    // Track cell instance move for bitmap layers
    if (layer->type() == Layer::BITMAP) {
        LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
        KeyFrame* kf = layer->getKeyFrameAt(mSrcFrame);
        if (kf) {
            BitmapKeyFrame* bitmapKF = dynamic_cast<BitmapKeyFrame*>(kf);
            if (bitmapKF && bitmapKF->cellId() != -1 && bitmapLayer->cellBank()) {
                bitmapLayer->cellBank()->moveInstance(bitmapKF->cellId(), mLayerId, mSrcFrame, mDstFrame);
            }
        }
    }
    
    layer->moveKeyFrame(mSrcFrame, offset);

    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->frameModified(mDstFrame);
    editor()->updateFrame();
    editor()->updateUndoContext();
}

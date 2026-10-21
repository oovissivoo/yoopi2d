#ifndef TIMELINECOMMANDS_H
#define TIMELINECOMMANDS_H

#include "undoredocommand.h"
#include "keyframe.h"

// Command to Add a Keyframe
class AddKeyFrameCommand : public UndoRedoCommand
{
public:
    AddKeyFrameCommand(int frameIndex, int layerId, Editor* editor);
    ~AddKeyFrameCommand() override;

    void undo() override;
    void redo() override;
    int id() const override { return 1001; } // Unique ID if needed for merging

private:
   // We might need to store the keyframe Type or data if we want to be fancy,
   // but for now, "removing" the keyframe at that index is the Undo.
   // Wait, if we undo add, we delete the keyframe. 
   // If we redo add, we create a new one. The original KeyFrame pointer is managed by Layer.
   // But if we delete it in Undo, we lose data if it wasn't empty?
   // addNewKey() adds an empty keyframe (or duplicate if requested, but this command is strictly ADD NEW EMPTY).
   
   // Actually, addNewKey() creates a keyframe.
   // If we undo, we remove it. 
   // If we redo, we add it back.
   // Ideally we shouldn't rely on "creating new" every time if we want to preserve pointer identity? 
   // Actually, pointer identity matters for the undo stack IF we had per-keyframe stacks. 
    // Since we are moving to GLOBAL stack, we don't care about keyframe pointer identity as long as data is consistent.
   
    // However, if we delete the keyframe on Undo, we need to make sure we don't leak anything.
};

// Command to Remove a Keyframe
class RemoveKeyFrameCommand : public UndoRedoCommand
{
public:
    RemoveKeyFrameCommand(int frameIndex, int layerId, Editor* editor);
    ~RemoveKeyFrameCommand() override;

    void undo() override;
    void redo() override;

private:
    KeyFrame* mBackupKeyFrame = nullptr; // To restore on Undo
};

// Command to Duplicate a Keyframe
class DuplicateKeyFrameCommand : public UndoRedoCommand
{
public:
    DuplicateKeyFrameCommand(int srcFrame, int dstFrame, int layerId, Editor* editor);
    ~DuplicateKeyFrameCommand() override { if (mBackupKey) delete mBackupKey; }

    void undo() override;
    void redo() override;
    int id() const override { return 1003; }

private:
    int mSrcFrame;
    int mDstFrame;
    KeyFrame* mBackupKey = nullptr; // In case we overwrite an existing key at dst
    // Actually duplicate usually happens on empty frame or overwrites? 
    // Usually duplicates to next empty or specific. 
    // We will assume overwrite logic if key exists, so backup needed.
};

// Command to Make a Drawing Unique (break shared cell reference, deep-copy the cell)
class MakeUniqueDrawingCommand : public UndoRedoCommand
{
public:
    MakeUniqueDrawingCommand(int frameIndex, int layerId, Editor* editor);
    ~MakeUniqueDrawingCommand() override;

    void undo() override;
    void redo() override;
    int id() const override { return 1005; }

private:
    int mOldCellId = -1;
    int mNewCellId = -1;
};

// Command to Move a Keyframe
class MoveKeyFrameCommand : public UndoRedoCommand
{
public:
    MoveKeyFrameCommand(int srcFrame, int dstFrame, int layerId, Editor* editor);
    // Destructor might need to handle backup if overwritten

    void undo() override;
    void redo() override;
    int id() const override { return 1004; }

private:
    int mSrcFrame;
    int mDstFrame;
    // Moving might overwrite? Logic depends on "Move" vs "Swap". 
    // Layer::moveKeyFrame usually handles swap or complex logic.
    // Simplifying to Move = Cut + Paste (Overwrite).
    // So we need to backup destination if it exists.
};

#endif // TIMELINECOMMANDS_H

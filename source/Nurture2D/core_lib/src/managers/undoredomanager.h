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
#ifndef UNDOREDOMANAGER_H
#define UNDOREDOMANAGER_H

#include "basemanager.h"
#include "layer.h"
#include "keyframe.h"
#include "bitmapimage.h"
#include "preferencesdef.h"

#include <QUndoStack>
#include <QRectF>
#include <QObject>

class QAction;
class QUndoCommand;

class BitmapImage;
class VectorImage;
class Camera;
class SoundClip;
class KeyFrame;
class LegacyBackupElement;
class UndoRedoCommand;

/// The undo/redo type which correspond to what is being recorded
enum class UndoRedoRecordType {
    KEYFRAME_MODIFY, // Any modification that involve a keyframe

    // Possible future actions
    // KEYFRAME_REMOVE, // Removing a keyframe
    // KEYFRAME_ADD, // Adding a keyframe
    // SCRUB_LAYER, // Scrubbing layer
    // SCRUB_KEYFRAME, // Scrubbing keyframe
    INVALID
};

struct SelectionSaveState {

    SelectionSaveState(const QRectF& rect,
                       const qreal rotationAngle,
                       const qreal scaleX,
                       const qreal scaleY,
                       const QPointF& translation,
                       const QPointF& anchor)
    {
        this->bounds = rect;
        this->rotationAngle = rotationAngle;
        this->scaleX = scaleX;
        this->scaleY = scaleY;
        this->translation = translation;
        this->anchor = anchor;
    }

    QRectF  bounds;
    qreal   rotationAngle = 0.0;
    qreal   scaleX = 0.0;
    qreal   scaleY = 0.0;
    QPointF translation;
    QPointF anchor;
};

/// This is the main undo/redo state structure which is meant to populate
/// whatever states that needs to be stored temporarily.
struct UndoSaveState {
    UndoSaveState();
    ~UndoSaveState();

    int layerId = 0;
    Layer::LAYER_TYPE layerType = Layer::UNDEFINED;

    std::unique_ptr<KeyFrame> keyframe = nullptr;
    std::unique_ptr<BitmapImage> bitmapImage = nullptr;
    std::unique_ptr<SelectionSaveState> selectionState = nullptr;

    UndoRedoRecordType recordType = UndoRedoRecordType::INVALID;
};

/**
 * UndoRedoManager
 * ---------------------------------------
 * Centralized manager for the application's undo/redo system with explicit
 * support for per-keyframe undo stacks (bitmap/vector) and a global fallback
 * stack. This class is designed with the following architecture and usage in mind:
 *
 * - Global vs per-keyframe stacks: A global QUndoStack (mUndoStack) is used when
 *   there is no active keyframe context (e.g. layer/keyframe add/remove). Each
 *   KeyFrame maintains its own QUndoStack created on demand; when a keyframe is
 *   active via setActiveKeyFrame(), all commands are pushed to that stack.
 *
 * - Ownership semantics for UndoSaveState: state() returns a heap-allocated
 *   const UndoSaveState* that MUST be passed to record(). The record() method
 *   takes ownership, consumes the state, deletes it and nullifies the caller's
 *   pointer (via reference). Callers must not delete or reuse it after record().
 *
 * - Threading: All APIs are MAIN THREAD ONLY. Tools must call record(),
 *   pushCommand(), and setActiveKeyFrame() from the GUI thread. Assertions are
 *   used to detect misuse from worker threads.
 *
 * - KeyFrame lifecycle: The manager implements KeyFrameEventListener to be
 *   notified before a keyframe is destroyed. If the active keyframe is about to
 *   be destroyed, the manager automatically switches back to the global stack to
 *   avoid dangling pointers and safely maintain undo history continuity.
 *
 * - Typical usage (bitmap-focused):
 *   1) const UndoSaveState* s = state(KEYFRAME_MODIFY)
 *   2) ...perform edits...
 *   3) record(s, "Tool Operation Description")
 */
class UndoRedoManager : public BaseManager
{
    Q_OBJECT

public:
    explicit UndoRedoManager(Editor* editor);
    ~UndoRedoManager() override;

    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;

    void record(const UndoSaveState*& undoState, const QString& description);

    bool hasUnsavedChanges() const;

    const UndoSaveState* state(UndoRedoRecordType recordType) const;

    QAction* createUndoAction(QObject* parent, const QIcon& icon);
    QAction* createRedoAction(QObject* parent, const QIcon& icon);

    bool eventFilter(QObject* watched, QEvent* event) override;

    void updateUndoAction(QAction* undoAction);
    void updateRedoAction(QAction* redoAction);

    void clearStack();

    QUndoStack* undoStack() { return &mGlobalStack; } // Global stack for timeline
    QUndoStack* activeStack();  // Returns active cell stack or global
    
    void setActiveCellStack(QUndoStack* stack);
    
    void pushCommand(QUndoCommand* command);
    
    void onSettingChanged(SETTING setting);
    void restoreLegacyKey();

signals:
    void didUpdateUndoStack();
    void activeStackChanged();  // Emitted when active stack switches
    // void activeStackChanged(); // Removed

public slots:
    void undo();
    void redo();
    void updateActions();

private:
    bool isActiveLayerLocked() const;
    void replaceKeyFrame(const UndoSaveState& undoState, const QString& description);
    void replaceBitmap(const UndoSaveState& undoState, const QString& description);
    void replaceVector(const UndoSaveState& undoState, const QString& description);

    const UndoSaveState* savedKeyFrameState() const;

    QUndoStack mGlobalStack;           // Timeline/layer operations
    QUndoStack* mActiveCellStack;      // Current cell being edited (or nullptr)
    
    QAction* mUndoAction = nullptr;
    QAction* mRedoAction = nullptr;
};

#endif // UNDOREDOMANAGER_H

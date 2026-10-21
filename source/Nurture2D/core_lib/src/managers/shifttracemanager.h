/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2025 Pencil2D Contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef SHIFTTRACEMANAGER_H
#define SHIFTTRACEMANAGER_H

#include <QObject>
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QVector>
#include <QtGlobal>

#include "basemanager.h"

class Editor;

/**
 * ShiftTraceManager manages a collection of user-picked frames (by layer+frame index)
 * that are displayed as manual onion skins with per-entry opacity, visibility and
 * a per-entry translation offset that does not affect real frame content.
 */
class ShiftTraceManager : public BaseManager
{
    Q_OBJECT
public:
    // Maximum number of manual shift & trace slots (10 per side in the dock UI)
    static constexpr int kMaxEntries = 20;

    struct Entry
    {
        int id = -1;                 // unique id within manager
        int layerIndex = -1;          // layer index where the keyframe lives
        int frameNumber = -1;         // absolute frame index (1-based)
        quint64 celKey = 0;           // stable identity of the underlying cel (for retiming)
        int exposureOffset = 0;       // exposure-relative offset: negative=previous exposures, positive=next exposures
        bool visible = true;          // shown/hidden
        int opacityPercent = 100;     // 0..100
        QPointF offset = QPointF(0, 0); // translation in canvas coordinates
        QPointF pivot = QPointF(0, 0);   // pivot in canvas coordinates
        QPointF defaultPivot = QPointF(0, 0);
        bool hasCustomPivot = false;
        qreal scale = 1.0;            // uniform scale
        qreal rotationDeg = 0.0;      // rotation in degrees
    };

    explicit ShiftTraceManager(Editor* editor);
    ~ShiftTraceManager() override;

    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;
    void workingLayerChanged(Layer*) override;

    // Entries
    const QVector<Entry>& entries() const { return mEntries; }
    bool hasEntry(int layerIndex, int frameNumber) const;
    void addEntry(int layerIndex, int frameNumber);
    void addEntries(const QVector<QPair<int,int>>& layerFramePairs);
    void removeByIds(const QVector<int>& ids);
    void clearAll();

    // Properties
    void setEntryVisible(int id, bool visible);
    void setEntryOpacity(int id, int opacityPercent);
    void setEntryOffset(int id, const QPointF& offset);
    void setEntryPivot(int id, const QPointF& canvasPivot);
    void resetEntryPivot(int id);
    void setEntryPivotAndOffset(int id, const QPointF& canvasPivot, const QPointF& newOffset);
    void setEntryScale(int id, qreal scale);
    void setEntryRotation(int id, qreal rotationDeg);
    void resetEntryTransform(int id);

    // HoldGhost: per-slot freeze/capture API
    void setSlotHeld(int slotIndex, bool held);
    inline bool isSlotHeld(int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= mSlotHeld.size()) return false;
        return mSlotHeld[slotIndex];
    }
    const QImage& heldImage(int slotIndex) const;
    QPoint heldTopLeft(int slotIndex) const;
    void initHoldState();

    // HoldGhost per-slot tint
    void setSlotTintColor(int slotIndex, const QColor& color);
    void setSlotTintEnabled(int slotIndex, bool enabled);
    QColor slotTintColor(int slotIndex) const;
    bool slotTintEnabled(int slotIndex) const;

    // Movement mode (only one entry can be moved at a time)
    int activeMoveEntryId() const { return mActiveMoveEntryId; }
    void setActiveMoveEntryId(int id);

    // Interactive drag helpers (canvas-space)
    enum class DragMode { None, Translate, Scale, Rotate };
    void beginDrag(const QPointF& canvasPos);
    void beginTransformDrag(DragMode mode, const QPointF& canvasPos);
    void updateDrag(const QPointF& canvasPos);
    void endDrag();

    // Global options
    bool tintEnabled() const { return mPrevTintEnabled || mNextTintEnabled; }
    void setTintEnabled(bool enabled);

    bool prevTintEnabled() const { return mPrevTintEnabled; }
    void setPrevTintEnabled(bool enabled);

    bool nextTintEnabled() const { return mNextTintEnabled; }
    void setNextTintEnabled(bool enabled);

    QColor prevTintColor() const { return mPrevTintColor; }
    void setPrevTintColor(const QColor& color);

    QColor nextTintColor() const { return mNextTintColor; }
    void setNextTintColor(const QColor& color);
    
    bool hideAll() const { return mHideAll; }
    void setHideAll(bool hide);

signals:
    void entriesChanged();
    void entryChanged(int id);
    void activeMoveEntryChanged(int id);
    void tintChanged(bool enabled);
    void needsRedraw();
    void hideAllChanged(bool hideAll);

private:
    enum class UpdateReason { Scrub, TimelineChange };
    void updateComputedFrames(UpdateReason reason);

    QVector<Entry> mEntries;
    int mNextId = 1;

    int mActiveMoveEntryId = -1;
    bool mIsDragging = false;
    DragMode mDragMode = DragMode::Translate;
    QPointF mDragStartCanvasPos; // canvas-space start position
    QPointF mDragStartOffset;    // offset at start for active entry
    QPointF mDragStartPivot;     // pivot at start for active entry
    qreal mDragStartScale = 1.0;
    qreal mDragStartRotationDeg = 0.0;
    qreal mScaleRefDistance = 0.0;
    qreal mRotationRefAngleDeg = 0.0;

    struct TransformState
    {
        QPointF offset = QPointF(0, 0);
        QPointF pivot = QPointF(0, 0);
        bool hasCustomPivot = false;
        qreal scale = 1.0;
        qreal rotationDeg = 0.0;
    };

    QVector<TransformState> mSlotTransforms;

    // HoldGhost parallel state (indexed by slot position, always kMaxEntries in size)
    QVector<bool>   mSlotHeld;       // true = slot is frozen
    QVector<QImage> mHeldImages;     // pixel copy captured at lock time; null when not held
    QVector<QPoint> mHeldTopLefts;   // canvas-space top-left of the captured bitmap
    QVector<QColor> mSlotTintColors; // per-slot tint color for held ghosts
    QVector<bool>   mSlotTintEnabled; // per-slot tint on/off

    bool mTintEnabled = false;
    bool mPrevTintEnabled = false;
    bool mNextTintEnabled = false;
    QColor mPrevTintColor = QColor(0, 0, 255);
    QColor mNextTintColor = QColor(255, 136, 0);
    bool mHideAll = false;
};

#endif // SHIFTTRACEMANAGER_H 
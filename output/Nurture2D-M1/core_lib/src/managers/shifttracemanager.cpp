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

#include "shifttracemanager.h"

#include "util/pencildef.h"

#include <QSet>
#include <QSettings>
#include <QtMath>
#include <cmath>
#include <algorithm>

#include "editor.h"
#include "structure/keyframe.h"
#include "structure/layer.h"
#include "structure/layerbitmap.h"
#include "structure/bitmapkeyframe.h"
#include "structure/layervector.h"
#include "structure/object.h"
#include "managers/layermanager.h"
#include "graphics/bitmap/bitmapimage.h"
#include "graphics/vector/vectorimage.h"

#include <QPainter>
#include <QDebug>

ShiftTraceManager::ShiftTraceManager(Editor* editor)
    : BaseManager(editor, "ShiftTraceManager")
{
}

ShiftTraceManager::~ShiftTraceManager() = default;

namespace {
inline int exposureOffsetForSlotIndex(int slotIndex)
{
    const int mid = ShiftTraceManager::kMaxEntries / 2;
    if (slotIndex < mid) {
        return -(mid - slotIndex);
    }
    return slotIndex - mid + 1;
}

inline bool isSlotMode(const QVector<ShiftTraceManager::Entry>& entries)
{
    if (entries.size() != ShiftTraceManager::kMaxEntries) return false;
    for (const auto& e : entries) {
        if (e.exposureOffset != 0) return true;
    }
    return false;
}

inline quint64 celIdentityKey(const Layer* layer, int frameNumber)
{
    if (!layer || frameNumber < 1) return 0;

    // For bitmap layers, use the shared cellId (stable across retiming).
    if (layer->type() == Layer::BITMAP) {
        KeyFrame* kf = layer->getLastKeyFrameAtPosition(frameNumber);
        if (!kf) return 0;
        const BitmapKeyFrame* bkf = static_cast<const BitmapKeyFrame*>(kf);
        const int cellId = bkf ? bkf->cellId() : -1;
        if (cellId < 0) return 0;
        const quint64 lid = static_cast<quint64>(static_cast<quint32>(layer->id()));
        const quint64 cid = static_cast<quint64>(static_cast<quint32>(cellId + 1));
        return (lid << 32) | cid;
    }

    // Fallback: use keyframe pointer identity (stable for that keyframe instance).
    KeyFrame* kf = layer->getLastKeyFrameAtPosition(frameNumber);
    return kf ? static_cast<quint64>(reinterpret_cast<quintptr>(kf)) : 0;
}

inline void resetSlotTransform(ShiftTraceManager::Entry& e)
{
    e.offset = QPointF(0, 0);
    e.scale = 1.0;
    e.rotationDeg = 0.0;
    e.hasCustomPivot = false;
    e.pivot = e.defaultPivot;
}
}

bool ShiftTraceManager::init()
{
    // Shift & Trace dock is a fixed 20-slot exposure-relative onion system.
    // If we detect legacy/variable entry mode, rebuild into slots.
    if (!isSlotMode(mEntries)) {
        mEntries.clear();
        mEntries.reserve(kMaxEntries);
        for (int i = 0; i < kMaxEntries; ++i) {
            Entry e;
            e.id = mNextId++;
            e.exposureOffset = exposureOffsetForSlotIndex(i);
            e.visible = false;
            e.opacityPercent = 70;
            mEntries.push_back(e);
        }
        emit entriesChanged();
    }

    // Load persisted slot state (visible + opacity per slot index).
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        const QString hasSavedKey = QStringLiteral("ShiftTrace/SlotStateSaved");
        const bool hasSaved = settings.value(hasSavedKey, false).toBool();

        if (hasSaved) {
            bool changed = false;
            for (int i = 0; i < mEntries.size(); ++i) {
                const QString visKey = QStringLiteral("ShiftTrace/Slot%1/visible").arg(i);
                const QString opKey  = QStringLiteral("ShiftTrace/Slot%1/opacity").arg(i);
                const bool vis = settings.value(visKey, false).toBool();
                const int  op  = qBound(0, settings.value(opKey, 70).toInt(), 100);
                if (mEntries[i].visible != vis || mEntries[i].opacityPercent != op) {
                    mEntries[i].visible = vis;
                    mEntries[i].opacityPercent = op;
                    changed = true;
                }
            }
            if (changed) {
                emit entriesChanged();
            }
        } else {
            // First-run defaults (one-time): enable the closest previous/next slots.
            const QString appliedKey = QStringLiteral("ShiftTrace/FirstRunDefaultsApplied");
            const bool alreadyApplied = settings.value(appliedKey, false).toBool();
            if (!alreadyApplied && isSlotMode(mEntries)) {
                const int mid = kMaxEntries / 2;
                bool changed = false;
                if (mid - 1 >= 0 && !mEntries[mid - 1].visible) { mEntries[mid - 1].visible = true; changed = true; }
                if (mid < mEntries.size() && !mEntries[mid].visible) { mEntries[mid].visible = true; changed = true; }
                settings.setValue(appliedKey, true);
                settings.sync();
                if (changed) { emit entriesChanged(); emit needsRedraw(); }
            }
        }
    }

    if (mSlotTransforms.size() != kMaxEntries) {
        mSlotTransforms.resize(kMaxEntries);
    }

    // Restore hideAll state
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        mHideAll = settings.value(QStringLiteral("ShiftTrace/hideAll"), false).toBool();
    }

    connect(editor(), &Editor::scrubbed, this, [this](int){
        updateComputedFrames(UpdateReason::Scrub);
    });
    connect(editor(), &Editor::frameModified, this, [this](int){
        updateComputedFrames(UpdateReason::Scrub);
    });
    connect(editor(), &Editor::framesModified, this, [this]{
        updateComputedFrames(UpdateReason::Scrub);
    });

    connect(editor(), &Editor::updateTimeLine, this, [this]{
        updateComputedFrames(UpdateReason::TimelineChange);
    });

    connect(editor(), &Editor::objectLoaded, this, [this]{
        updateComputedFrames(UpdateReason::TimelineChange);
    });

    if (editor()->object() != nullptr) {
        updateComputedFrames(UpdateReason::TimelineChange);
    }

    initHoldState();
    return true;
}

void ShiftTraceManager::initHoldState()
{
    mSlotHeld.resize(kMaxEntries);
    mSlotHeld.fill(false);
    mHeldImages.resize(kMaxEntries);
    mHeldImages.fill(QImage());
    mHeldTopLefts.resize(kMaxEntries);
    mHeldTopLefts.fill(QPoint(0, 0));
    mSlotTintColors.resize(kMaxEntries);
    mSlotTintColors.fill(QColor(0x40, 0x55, 0x5c, 0xff)); // default: cosmic accent
    mSlotTintEnabled.resize(kMaxEntries);
    mSlotTintEnabled.fill(false);
}

void ShiftTraceManager::setSlotHeld(int slotIndex, bool held)
{
    if (slotIndex < 0 || slotIndex >= kMaxEntries) return;
    if (slotIndex >= mEntries.size()) return;

    Entry& e = mEntries[slotIndex];

    if (held) {
        // Lock the CURRENT frame into this slot as a pixel copy.
        // Reads from the live layer now, then is fully isolated from any future edits.
        Layer* layer = editor()->layers()->currentLayer();
        if (!layer) return;

        const int currentFrame = editor()->currentFrame();
        KeyFrame* kf = layer->getLastKeyFrameAtPosition(currentFrame);
        if (!kf || kf->pos() < 1) return;

        const int framePos = kf->pos();
        const int layerIndex = editor()->layers()->currentLayerIndex();

        QImage captured;
        QPoint capturedTopLeft(0, 0);

        if (layer->type() == Layer::BITMAP) {
            LayerBitmap* bl = static_cast<LayerBitmap*>(layer);
            BitmapImage* img = bl->getBitmapImageAtFrame(framePos);
            if (!img) return;
            img->loadFile();
            const QImage* src = img->image();
            if (!src || src->isNull()) return;
            captured = src->copy();           // deep copy — isolated from future edits
            capturedTopLeft = img->topLeft(); // preserve canvas-space position
        } else if (layer->type() == Layer::VECTOR) {
            LayerVector* vl = static_cast<LayerVector*>(layer);
            VectorImage* vimg = vl->getVectorImageAtFrame(framePos);
            if (!vimg) return;
            QSize sz = vimg->getSize();
            if (sz.isEmpty()) sz = QSize(1, 1);
            captured = QImage(sz, QImage::Format_ARGB32_Premultiplied);
            captured.fill(Qt::transparent);
            QPainter p(&captured);
            p.setRenderHint(QPainter::Antialiasing, true);
            Object* obj = editor()->object();
            if (!obj) return;
            vimg->paintImage(p, *obj, false, false, true);
            p.end();
            capturedTopLeft = QPoint(0, 0);
        } else {
            return;
        }

        if (captured.isNull()) return;

        // Stamp entry metadata so transform/visibility work correctly
        e.layerIndex = layerIndex;
        e.frameNumber = framePos;
        e.celKey = celIdentityKey(layer, framePos);
        e.visible = true;

        // Recompute pivot from frame bounds
        QRect bounds;
        if (layer->type() == Layer::BITMAP) {
            bounds = static_cast<LayerBitmap*>(layer)->getFrameBounds(framePos);
        } else if (layer->type() == Layer::VECTOR) {
            VectorImage* vimg = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(framePos);
            if (vimg) {
                QRectF rectF = vimg->getBoundsOfTransformedCurves();
                bounds = rectF.toAlignedRect();
                if (bounds.isEmpty()) bounds = QRect(QPoint(0,0), vimg->getSize());
            }
        }
        e.defaultPivot = bounds.isEmpty() ? QPointF(0,0) : QPointF(bounds.center());
        if (!e.hasCustomPivot) e.pivot = e.defaultPivot;

        mHeldImages[slotIndex] = std::move(captured);
        mHeldTopLefts[slotIndex] = capturedTopLeft;
        mSlotHeld[slotIndex] = true;
    } else {
        mHeldImages[slotIndex] = QImage();
        mHeldTopLefts[slotIndex] = QPoint(0, 0);
        mSlotHeld[slotIndex] = false;
        // Resume live tracking
        updateComputedFrames(UpdateReason::Scrub);
    }

    emit entryChanged(e.id);
    emit needsRedraw();
}

static const QImage kNullImage;

const QImage& ShiftTraceManager::heldImage(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= mHeldImages.size()) return kNullImage;
    return mHeldImages[slotIndex];
}

QPoint ShiftTraceManager::heldTopLeft(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= mHeldTopLefts.size()) return QPoint(0, 0);
    return mHeldTopLefts[slotIndex];
}

void ShiftTraceManager::setSlotTintColor(int slotIndex, const QColor& color)
{
    if (slotIndex < 0 || slotIndex >= kMaxEntries) return;
    mSlotTintColors[slotIndex] = color;
    if (mSlotHeld[slotIndex]) emit needsRedraw();
}

void ShiftTraceManager::setSlotTintEnabled(int slotIndex, bool enabled)
{
    if (slotIndex < 0 || slotIndex >= kMaxEntries) return;
    mSlotTintEnabled[slotIndex] = enabled;
    if (mSlotHeld[slotIndex]) emit needsRedraw();
}

QColor ShiftTraceManager::slotTintColor(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= mSlotTintColors.size()) return QColor(0x40, 0x55, 0x5c);
    return mSlotTintColors[slotIndex];
}

bool ShiftTraceManager::slotTintEnabled(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= mSlotTintEnabled.size()) return false;
    return mSlotTintEnabled[slotIndex];
}


Status ShiftTraceManager::load(Object*)
{
    // Slot visibility and opacity are persisted via QSettings in init().
    return Status::OK;
}

Status ShiftTraceManager::save(Object*)
{
    // Slot state is saved immediately on change via QSettings.
    return Status::OK;
}

void ShiftTraceManager::workingLayerChanged(Layer*)
{
    updateComputedFrames(UpdateReason::Scrub);
}

void ShiftTraceManager::updateComputedFrames(UpdateReason reason)
{
    if (!isSlotMode(mEntries)) return;
    Q_UNUSED(reason);
    if (mSlotTransforms.size() != mEntries.size()) {
        mSlotTransforms.resize(mEntries.size());
    }

    auto applySlotTransform = [this](int slotIndex, Entry& e, bool updateDefaultPivot) {
        if (slotIndex < 0 || slotIndex >= mSlotTransforms.size()) return;
        TransformState& st = mSlotTransforms[slotIndex];
        if (updateDefaultPivot && !st.hasCustomPivot) {
            st.pivot = e.defaultPivot;
        }
        e.offset = st.offset;
        e.scale = qBound<qreal>(0.01, st.scale, 1000.0);
        e.rotationDeg = st.rotationDeg;
        e.hasCustomPivot = st.hasCustomPivot;
        e.pivot = st.hasCustomPivot ? st.pivot : e.defaultPivot;
    };

    // During application startup, managers are initialized before an object is loaded.
    // LayerManager's accessors assert when there's no object, so guard this path.
    if (editor()->object() == nullptr) {
        const int layerIndex = editor()->currentLayerIndex();
        for (int i = 0; i < mEntries.size(); ++i) {
            if (i < mSlotHeld.size() && mSlotHeld[i]) continue; // held — completely frozen
            Entry& e = mEntries[i];
            e.layerIndex = layerIndex;
            e.frameNumber = -1;
            e.celKey = 0;
            applySlotTransform(i, e, false);
        }
        emit needsRedraw();
        return;
    }

    const int layerIndex = editor()->layers()->currentLayerIndex();
    Layer* layer = editor()->layers()->currentLayer();
    if (!layer) {
        for (int i = 0; i < mEntries.size(); ++i) {
            if (i < mSlotHeld.size() && mSlotHeld[i]) continue; // held — completely frozen
            Entry& e = mEntries[i];
            e.layerIndex = layerIndex;
            e.frameNumber = -1;
            e.celKey = 0;
            applySlotTransform(i, e, false);
        }
        emit needsRedraw();
        return;
    }

    const int currentFrame = editor()->currentFrame();
    KeyFrame* curKf = layer->getLastKeyFrameAtPosition(currentFrame);
    const int basePos = curKf ? curKf->pos() : -1;

    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];

        // Held slots are completely frozen — skip all recomputation including transforms.
        if (i < mSlotHeld.size() && mSlotHeld[i]) continue;

        e.layerIndex = layerIndex;

        if (basePos < 1 || e.exposureOffset == 0) {
            e.frameNumber = -1;
            e.celKey = 0;
            applySlotTransform(i, e, false);
            continue;
        }

        int pos = basePos;
        const int steps = std::abs(e.exposureOffset);
        if (e.exposureOffset < 0) {
            for (int i = 0; i < steps; ++i) {
                pos = layer->getPreviousFrameNumber(pos, true);
                if (pos < 1) break;
            }
        } else {
            for (int i = 0; i < steps; ++i) {
                pos = layer->getNextFrameNumber(pos, true);
                if (pos < 1) break;
            }
        }

        e.frameNumber = (pos >= 1) ? pos : -1;

        if (e.frameNumber < 1) {
            e.celKey = 0;
            applySlotTransform(i, e, false);
            continue;
        }

        // Always recompute default pivot for the current exposure.
        QRect bounds;
        if (layer->type() == Layer::BITMAP) {
            bounds = static_cast<LayerBitmap*>(layer)->getFrameBounds(e.frameNumber);
        } else if (layer->type() == Layer::VECTOR) {
            VectorImage* vimg = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(e.frameNumber);
            if (vimg) {
                QRectF rectF = vimg->getBoundsOfTransformedCurves();
                bounds = rectF.toAlignedRect();
                if (bounds.isEmpty()) bounds = QRect(QPoint(0,0), vimg->getSize());
            }
        }
        const QPointF center = bounds.isEmpty() ? QPointF(0,0) : QPointF(bounds.center());
        e.defaultPivot = center;

        const quint64 newCelKey = celIdentityKey(layer, e.frameNumber);
        e.celKey = newCelKey;
        applySlotTransform(i, e, true);
    }

    emit needsRedraw();
}

bool ShiftTraceManager::hasEntry(int layerIndex, int frameNumber) const
{
    for (const Entry& e : mEntries) {
        if (e.layerIndex == layerIndex && e.frameNumber == frameNumber) return true;
    }
    return false;
}

void ShiftTraceManager::addEntry(int layerIndex, int frameNumber)
{
    if (isSlotMode(mEntries)) return;
    if (layerIndex < 0 || frameNumber < 1) return;
    if (mEntries.size() >= kMaxEntries) return;
    if (hasEntry(layerIndex, frameNumber)) return;

    Entry e;
    e.id = mNextId++;
    e.layerIndex = layerIndex;
    e.frameNumber = frameNumber;
    e.opacityPercent = 70;
    // Initialize default pivot to the center of frame bounds if available
    Layer* layer = editor()->layers()->getLayer(layerIndex);
    if (layer && (layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR)) {
        QRect bounds;
        if (layer->type() == Layer::BITMAP) {
            bounds = static_cast<LayerBitmap*>(layer)->getFrameBounds(frameNumber);
        } else {
            // Vector frame bounds: approximate via selection rect if present, else use object size; fallback 0
            VectorImage* vimg = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(frameNumber);
            if (vimg) {
                QRectF rectF = vimg->getBoundsOfTransformedCurves();
                bounds = rectF.toAlignedRect();
                if (bounds.isEmpty()) bounds = QRect(QPoint(0,0), vimg->getSize());
            }
        }
        QPointF center = bounds.isEmpty() ? QPointF(0,0) : QPointF(bounds.center());
        e.pivot = center;
        e.defaultPivot = center;
        e.hasCustomPivot = false;
    }
    mEntries.push_back(e);
    emit entriesChanged();
    emit needsRedraw();
}

void ShiftTraceManager::addEntries(const QVector<QPair<int,int>>& layerFramePairs)
{
    if (isSlotMode(mEntries)) return;
    int availableSlots = kMaxEntries - mEntries.size();
    if (availableSlots <= 0) return;

    bool changed = false;
    int addedCount = 0;
    for (const auto& p : layerFramePairs) {
        if (addedCount >= availableSlots) break;
        if (!hasEntry(p.first, p.second)) {
            Entry e;
            e.id = mNextId++;
            e.layerIndex = p.first;
            e.frameNumber = p.second;
            e.opacityPercent = 70;
            Layer* layer = editor()->layers()->getLayer(e.layerIndex);
            if (layer && (layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR)) {
                QRect bounds;
                if (layer->type() == Layer::BITMAP) {
                    bounds = static_cast<LayerBitmap*>(layer)->getFrameBounds(e.frameNumber);
                } else {
                    VectorImage* vimg = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(e.frameNumber);
                    if (vimg) {
                        QRectF rectF = vimg->getBoundsOfTransformedCurves();
                        bounds = rectF.toAlignedRect();
                        if (bounds.isEmpty()) bounds = QRect(QPoint(0,0), vimg->getSize());
                    }
                }
                QPointF center = bounds.isEmpty() ? QPointF(0,0) : QPointF(bounds.center());
                e.pivot = center;
                e.defaultPivot = center;
                e.hasCustomPivot = false;
            }
            mEntries.push_back(e);
            changed = true;
            addedCount++;
        }
    }
    if (changed) emit entriesChanged();
    if (changed) emit needsRedraw();
}

void ShiftTraceManager::removeByIds(const QVector<int>& ids)
{
    if (ids.isEmpty()) return;
    if (isSlotMode(mEntries)) {
        for (int id : ids) {
            setEntryVisible(id, false);
        }
        return;
    }
    QSet<int> toRemove = QSet<int>(ids.cbegin(), ids.cend());
    int before = mEntries.size();
    mEntries.erase(std::remove_if(mEntries.begin(), mEntries.end(), [&](const Entry& e){ return toRemove.contains(e.id); }), mEntries.end());
    if (mActiveMoveEntryId != -1 && toRemove.contains(mActiveMoveEntryId)) {
        mActiveMoveEntryId = -1;
        emit activeMoveEntryChanged(-1);
    }
    if (mEntries.size() != before) emit entriesChanged();
    if (mEntries.size() != before) emit needsRedraw();
}

void ShiftTraceManager::clearAll()
{
    if (mEntries.isEmpty()) return;
    if (isSlotMode(mEntries)) {
        for (Entry& e : mEntries) {
            e.visible = false;
        }
        if (mActiveMoveEntryId != -1) {
            mActiveMoveEntryId = -1;
            emit activeMoveEntryChanged(-1);
        }
        emit entriesChanged();
        emit needsRedraw();
        return;
    }

    mEntries.clear();
    mActiveMoveEntryId = -1;
    emit entriesChanged();
    emit activeMoveEntryChanged(-1);
    emit needsRedraw();
}

void ShiftTraceManager::setEntryVisible(int id, bool visible)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            if (e.visible != visible) {
                e.visible = visible;
                if (visible) {
                    updateComputedFrames(UpdateReason::Scrub);
                }
                // Persist slot state
                QSettings settings(PENCIL2D, PENCIL2D);
                settings.setValue(QStringLiteral("ShiftTrace/Slot%1/visible").arg(i), visible);
                settings.setValue(QStringLiteral("ShiftTrace/SlotStateSaved"), true);
                emit entryChanged(id);
                emit needsRedraw();
            }
            return;
        }
    }
}

void ShiftTraceManager::setEntryOpacity(int id, int opacityPercent)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            opacityPercent = qBound(0, opacityPercent, 100);
            if (e.opacityPercent != opacityPercent) {
                e.opacityPercent = opacityPercent;
                // Persist slot state
                QSettings settings(PENCIL2D, PENCIL2D);
                settings.setValue(QStringLiteral("ShiftTrace/Slot%1/opacity").arg(i), opacityPercent);
                settings.setValue(QStringLiteral("ShiftTrace/SlotStateSaved"), true);
                emit entryChanged(id);
                emit needsRedraw();
            }
            return;
        }
    }
}

void ShiftTraceManager::setEntryOffset(int id, const QPointF& offset)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            if (e.offset != offset) {
                e.offset = offset;
                if (i >= 0 && i < mSlotTransforms.size()) {
                    TransformState& st = mSlotTransforms[i];
                    st.offset = e.offset;
                    st.pivot = e.pivot;
                    st.hasCustomPivot = e.hasCustomPivot;
                    st.scale = e.scale;
                    st.rotationDeg = e.rotationDeg;
                }
                emit entryChanged(id);
                emit needsRedraw();
            }
            return;
        }
    }
}

void ShiftTraceManager::setEntryPivot(int id, const QPointF& canvasPivot)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            if (!e.hasCustomPivot || e.pivot != canvasPivot) {
                e.pivot = canvasPivot;
                e.hasCustomPivot = true;
                if (i >= 0 && i < mSlotTransforms.size()) {
                    TransformState& st = mSlotTransforms[i];
                    st.offset = e.offset;
                    st.pivot = e.pivot;
                    st.hasCustomPivot = e.hasCustomPivot;
                    st.scale = e.scale;
                    st.rotationDeg = e.rotationDeg;
                }
                emit entryChanged(id);
                emit needsRedraw();
            }
            return;
        }
    }
}

void ShiftTraceManager::resetEntryPivot(int id)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            if (e.hasCustomPivot) {
                e.hasCustomPivot = false;
                e.pivot = e.defaultPivot;
                if (i >= 0 && i < mSlotTransforms.size()) {
                    TransformState& st = mSlotTransforms[i];
                    st.offset = e.offset;
                    st.pivot = e.pivot;
                    st.hasCustomPivot = e.hasCustomPivot;
                    st.scale = e.scale;
                    st.rotationDeg = e.rotationDeg;
                }
                emit entryChanged(id);
                emit needsRedraw();
            }
            return;
        }
    }
}

void ShiftTraceManager::setEntryPivotAndOffset(int id, const QPointF& canvasPivot, const QPointF& newOffset)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            const bool pivotChanged = (!e.hasCustomPivot || e.pivot != canvasPivot);
            const bool offsetChanged = (e.offset != newOffset);
            if (pivotChanged || offsetChanged) {
                e.pivot = canvasPivot;
                e.hasCustomPivot = true;
                e.offset = newOffset;
                if (i >= 0 && i < mSlotTransforms.size()) {
                    TransformState& st = mSlotTransforms[i];
                    st.offset = e.offset;
                    st.pivot = e.pivot;
                    st.hasCustomPivot = e.hasCustomPivot;
                    st.scale = e.scale;
                    st.rotationDeg = e.rotationDeg;
                }
                emit entryChanged(id);
                emit needsRedraw();
            }
            return;
        }
    }
}

void ShiftTraceManager::resetEntryTransform(int id)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            resetSlotTransform(e);

            if (i >= 0 && i < mSlotTransforms.size()) {
                TransformState& st = mSlotTransforms[i];
                st.offset = e.offset;
                st.pivot = e.pivot;
                st.hasCustomPivot = e.hasCustomPivot;
                st.scale = e.scale;
                st.rotationDeg = e.rotationDeg;
            }

            emit entryChanged(id);
            emit needsRedraw();
            return;
        }
    }
}

void ShiftTraceManager::setActiveMoveEntryId(int id)
{
    if (mActiveMoveEntryId == id) return;
    mActiveMoveEntryId = id;
    emit activeMoveEntryChanged(id);
    emit needsRedraw();
}

void ShiftTraceManager::beginDrag(const QPointF& canvasPos)
{
    if (mActiveMoveEntryId == -1) return;
    mIsDragging = true;
    mDragStartCanvasPos = canvasPos;
    for (const Entry& e : mEntries) {
        if (e.id == mActiveMoveEntryId) {
            mDragStartOffset = e.offset;
            mDragStartPivot = e.pivot;
            mDragStartScale = e.scale;
            mDragStartRotationDeg = e.rotationDeg;
            break;
        }
    }
}

void ShiftTraceManager::beginTransformDrag(DragMode mode, const QPointF& canvasPos)
{
    if (mActiveMoveEntryId == -1) return;
    mDragMode = mode;
    beginDrag(canvasPos);
    if (mDragMode == DragMode::Scale || mDragMode == DragMode::Rotate) {
        // Initialize reference metrics relative to pivot
        for (const Entry& e : mEntries) {
            if (e.id == mActiveMoveEntryId) {
                QPointF startVec = mDragStartCanvasPos - e.pivot;
                mScaleRefDistance = std::hypot(startVec.x(), startVec.y());
                mRotationRefAngleDeg = qRadiansToDegrees(std::atan2(startVec.y(), startVec.x()));
                break;
            }
        }
    }
}

void ShiftTraceManager::updateDrag(const QPointF& canvasPos)
{
    if (!mIsDragging || mActiveMoveEntryId == -1) return;
    QPointF delta = canvasPos - mDragStartCanvasPos;
    Entry* target = nullptr;
    for (Entry& e : mEntries) if (e.id == mActiveMoveEntryId) { target = &e; break; }
    if (target == nullptr) return;

    switch (mDragMode) {
    case DragMode::Translate:
        setEntryOffset(mActiveMoveEntryId, mDragStartOffset + delta);
        // Keep pivot in sync with translation so it follows the image
        setEntryPivot(mActiveMoveEntryId, mDragStartPivot + delta);
        break;
    case DragMode::Scale:
    {
        // Uniform scale based on radial distance change from pivot
        QPointF curVec = canvasPos - mDragStartPivot;
        qreal curDist = std::hypot(curVec.x(), curVec.y());
        qreal refDist = qMax<qreal>(0.0001, mScaleRefDistance);
        qreal factor = curDist / refDist;
        setEntryScale(mActiveMoveEntryId, mDragStartScale * factor);
        break;
    }
    case DragMode::Rotate:
    {
        // Angle delta relative to pivot
        QPointF curVec = canvasPos - mDragStartPivot;
        qreal curAngleDeg = qRadiansToDegrees(std::atan2(curVec.y(), curVec.x()));
        qreal deltaDeg = curAngleDeg - mRotationRefAngleDeg;
        setEntryRotation(mActiveMoveEntryId, mDragStartRotationDeg + deltaDeg);
        break;
    }
    default:
        break;
    }
    emit needsRedraw();
}

void ShiftTraceManager::endDrag()
{
    mIsDragging = false;
    mDragMode = DragMode::Translate;
}

void ShiftTraceManager::setTintEnabled(bool enabled)
{
    const bool prev = enabled;
    const bool next = enabled;
    if (mPrevTintEnabled == prev && mNextTintEnabled == next) return;
    mPrevTintEnabled = prev;
    mNextTintEnabled = next;
    mTintEnabled = (mPrevTintEnabled || mNextTintEnabled);
    emit tintChanged(mTintEnabled);
    emit needsRedraw();
}

void ShiftTraceManager::setPrevTintEnabled(bool enabled)
{
    if (mPrevTintEnabled == enabled) return;
    mPrevTintEnabled = enabled;
    mTintEnabled = (mPrevTintEnabled || mNextTintEnabled);
    emit tintChanged(mTintEnabled);
    emit needsRedraw();
}

void ShiftTraceManager::setNextTintEnabled(bool enabled)
{
    if (mNextTintEnabled == enabled) return;
    mNextTintEnabled = enabled;
    mTintEnabled = (mPrevTintEnabled || mNextTintEnabled);
    emit tintChanged(mTintEnabled);
    emit needsRedraw();
}

void ShiftTraceManager::setPrevTintColor(const QColor& color)
{
    if (mPrevTintColor == color) return;
    mPrevTintColor = color;
    emit needsRedraw();
}

void ShiftTraceManager::setNextTintColor(const QColor& color)
{
    if (mNextTintColor == color) return;
    mNextTintColor = color;
    emit needsRedraw();
}

void ShiftTraceManager::setHideAll(bool hide)
{
    if (mHideAll == hide) return;
    mHideAll = hide;
    qInfo() << "[OnionSkin] Master visibility changed; enabled=" << !hide;
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(QStringLiteral("ShiftTrace/hideAll"), hide);
    emit hideAllChanged(hide);
    emit needsRedraw();
}

void ShiftTraceManager::setEntryScale(int id, qreal scale)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            scale = qBound<qreal>(0.01, scale, 1000.0);
            if (!qFuzzyCompare(e.scale, scale)) {
                e.scale = scale;
                if (i >= 0 && i < mSlotTransforms.size()) {
                    TransformState& st = mSlotTransforms[i];
                    st.offset = e.offset;
                    st.pivot = e.pivot;
                    st.hasCustomPivot = e.hasCustomPivot;
                    st.scale = e.scale;
                    st.rotationDeg = e.rotationDeg;
                }
                emit entryChanged(id);
                emit needsRedraw();
            }
            return;
        }
    }
}

void ShiftTraceManager::setEntryRotation(int id, qreal rotationDeg)
{
    for (int i = 0; i < mEntries.size(); ++i) {
        Entry& e = mEntries[i];
        if (e.id == id) {
            if (!qFuzzyCompare(e.rotationDeg, rotationDeg)) {
                e.rotationDeg = rotationDeg;
                if (i >= 0 && i < mSlotTransforms.size()) {
                    TransformState& st = mSlotTransforms[i];
                    st.offset = e.offset;
                    st.pivot = e.pivot;
                    st.hasCustomPivot = e.hasCustomPivot;
                    st.scale = e.scale;
                    st.rotationDeg = e.rotationDeg;
                }
                emit entryChanged(id);
                emit needsRedraw();
            }
            return;
        }
    }
}

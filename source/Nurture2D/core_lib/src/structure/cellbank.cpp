/*
Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.
*/

#include "cellbank.h"
#include "bitmapimage.h"
#include "celllayerstack.h"
#include "innerlayer.h"
#include <QDebug>
#include <QUndoStack>
#include <QRegularExpression>

int CellBank::sGlobalUndoLimit = 0;
QList<CellBank*> CellBank::sAllBanks;
quint64 CellBank::sBitmapCacheLimitBytes = 512ull * 1024ull * 1024ull;

CellBank::CellBank(QObject* parent) : QObject(parent)
{
    sAllBanks.append(this);
}

CellBank::~CellBank()
{
    sAllBanks.removeAll(this);
    qDeleteAll(mCellStacks);
    mCellStacks.clear();
    mLRU.clear();
    mLRUIter.clear();
}

int CellBank::addCell(BitmapImage* cell, const QString& name)
{
    if (!cell) return -1;

    int id = mNextId++;
    mCellStacks.insert(id, new CellLayerStack(cell));
    mCellOrder.append(id);

    QString finalName = name;
    if (finalName.isEmpty()) {
        finalName = QString::number(id);
    }
    mCellNames.insert(id, finalName);

    emit cellAdded(id);
    touchCell(id);
    return id;
}

int CellBank::addCellStack(CellLayerStack* stack, const QString& name, int cellId)
{
    if (!stack || (cellId > 0 && mCellStacks.contains(cellId))) return -1;
    const int id = cellId > 0 ? cellId : mNextId;
    mNextId = qMax(mNextId, id + 1);
    mCellStacks.insert(id, stack);
    mCellOrder.append(id);
    mCellNames.insert(id, name);
    emit cellAdded(id);
    touchCell(id);
    return id;
}

int CellBank::addCellWithId(int cellId, BitmapImage* cell, const QString& name)
{
    if (!cell) return -1;
    if (cellId <= 0) {
        return addCell(cell, name);
    }

    if (mCellStacks.contains(cellId)) {
        return -1;
    }

    mCellStacks.insert(cellId, new CellLayerStack(cell));
    mCellOrder.append(cellId);

    QString finalName = name;
    if (finalName.isEmpty()) {
        finalName = QString::number(cellId);
    }
    mCellNames.insert(cellId, finalName);

    if (cellId >= mNextId) {
        mNextId = cellId + 1;
    }

    emit cellAdded(cellId);
    touchCell(cellId);
    return cellId;
}

void CellBank::removeCell(int id)
{
    if (mCellStacks.contains(id)) {
        delete mCellStacks.take(id);  // CellLayerStack owns all InnerLayers and their QUndoStacks
        mCellNames.remove(id);
        mCellOrder.removeAll(id);
        if (mLRUIter.contains(id)) {
            mLRU.erase(mLRUIter.value(id));
            mLRUIter.remove(id);
        }
        emit cellRemoved(id);
    }
}

int CellBank::duplicateCell(int id)
{
    CellLayerStack* original = mCellStacks.value(id, nullptr);
    if (!original) return -1;

    CellLayerStack* copy = original->deepCopy();
    int newId = mNextId++;
    mCellStacks.insert(newId, copy);
    mCellOrder.append(newId);

    QString newName = getCellName(id) + "_copy";
    mCellNames.insert(newId, newName);

    emit cellAdded(newId);
    touchCell(newId);
    return newId;
}

BitmapImage* CellBank::getCell(int id) const
{
    CellLayerStack* stack = mCellStacks.value(id, nullptr);
    if (!stack) return nullptr;
    const_cast<CellBank*>(this)->touchCell(id);
    return stack->composite();
}

QString CellBank::getCellName(int id) const
{
    return mCellNames.value(id);
}

void CellBank::setCellName(int id, const QString& name)
{
    if (mCellNames.contains(id)) {
        mCellNames[id] = name;
        emit cellRenamed(id, name);
    }
}

QList<int> CellBank::getAllCellIds() const
{
    if (!mCellOrder.isEmpty()) {
        QList<int> ordered;
        ordered.reserve(mCellStacks.size());

        for (int id : mCellOrder) {
            if (mCellStacks.contains(id)) {
                ordered.append(id);
            }
        }

        const QList<int> allIds = mCellStacks.keys();
        for (int id : allIds) {
            if (!ordered.contains(id)) {
                ordered.append(id);
            }
        }

        return ordered;
    }

    return mCellStacks.keys();
}

int CellBank::indexForCellId(int cellId) const
{
    const QList<int> ordered = getAllCellIds();
    int idx = ordered.indexOf(cellId);
    return (idx == -1) ? -1 : (idx + 1);
}

int CellBank::cellIdForIndex(int index) const
{
    if (index <= 0) return -1;
    const QList<int> ordered = getAllCellIds();
    if (index > ordered.size()) return -1;
    return ordered.at(index - 1);
}

void CellBank::setCellOrder(const QList<int>& order)
{
    QList<int> newOrder;
    newOrder.reserve(mCellStacks.size());

    for (int id : order) {
        if (mCellStacks.contains(id) && !newOrder.contains(id)) {
            newOrder.append(id);
        }
    }

    const QList<int> allIds = mCellStacks.keys();
    for (int id : allIds) {
        if (!newOrder.contains(id)) {
            newOrder.append(id);
        }
    }

    mCellOrder = newOrder;
}

void CellBank::touchCell(int cellId)
{
    if (cellId <= 0 || !mCellStacks.contains(cellId)) {
        return;
    }

    if (mLRUIter.contains(cellId)) {
        mLRU.erase(mLRUIter.value(cellId));
        mLRUIter.remove(cellId);
    }

    mLRU.append(cellId);
    auto it = mLRU.end();
    --it;
    mLRUIter.insert(cellId, it);

    trimBitmapCache(cellId);
}

void CellBank::setBitmapCacheLimitBytes(quint64 bytes)
{
    const quint64 minBytes = 64ull * 1024ull * 1024ull;
    sBitmapCacheLimitBytes = (bytes < minBytes) ? minBytes : bytes;
    for (CellBank* bank : sAllBanks) {
        if (bank) {
            bank->trimBitmapCache(-1);
        }
    }
}

quint64 CellBank::bitmapCacheLimitBytes()
{
    return sBitmapCacheLimitBytes;
}

void CellBank::trimBitmapCache(int protectedCellId)
{
    quint64 total = 0;
    for (CellBank* bank : sAllBanks) {
        if (!bank) continue;
        for (auto it = bank->mCellStacks.constBegin(); it != bank->mCellStacks.constEnd(); ++it) {
            CellLayerStack* stack = it.value();
            if (!stack) continue;
            for (int i = 0; i < stack->layerCount(); ++i) {
                const InnerLayer* layer = stack->layerAt(i);
                if (layer && layer->image && layer->image->isLoaded()) {
                    total += layer->image->memoryUsage();
                }
            }
        }
    }

    if (total <= sBitmapCacheLimitBytes) {
        return;
    }

    bool madeProgress = true;
    while (total > sBitmapCacheLimitBytes && madeProgress) {
        madeProgress = false;

        for (CellBank* bank : sAllBanks) {
            if (!bank || bank->mLRU.isEmpty()) continue;

            auto it = bank->mLRU.begin();
            while (it != bank->mLRU.end() && total > sBitmapCacheLimitBytes) {
                const int victimId = *it;
                if (victimId == protectedCellId) {
                    ++it;
                    continue;
                }

                CellLayerStack* stack = bank->mCellStacks.value(victimId, nullptr);
                if (!stack) { ++it; continue; }

                for (int i = 0; i < stack->layerCount(); ++i) {
                    InnerLayer* layer = stack->layerAt(i);
                    if (!layer || !layer->image || !layer->image->isLoaded() || layer->image->isModified()) continue;

                    const quint64 before = layer->image->memoryUsage();
                    layer->image->unloadFile();
                    const quint64 after = layer->image->memoryUsage();
                    if (before > after) {
                        const quint64 freed = before - after;
                        total = (freed > total) ? 0 : total - freed;
                        madeProgress = true;
                    }
                }

                bank->mLRUIter.remove(victimId);
                it = bank->mLRU.erase(it);
            }
        }
    }
}

QString CellBank::generateNextName(const QString& prefix)
{
    // Strip any trailing _NNN suffix so we work from the base name
    QString base = prefix;
    static const QRegularExpression trailNum(QStringLiteral("_\\d+$"));
    base.remove(trailNum);

    // Find the highest existing counter for this base
    int maxNum = 0;
    for (const QString& name : qAsConst(mCellNames)) {
        if (name.startsWith(base)) {
            const QString suffix = name.mid(base.length());
            if (suffix.startsWith(QLatin1Char('_'))) {
                bool ok = false;
                int n = suffix.mid(1).toInt(&ok);
                if (ok && n > maxNum) maxNum = n;
            }
        }
    }
    return QString("%1_%2").arg(base).arg(maxNum + 1, 3, 10, QChar('0'));
}

QUndoStack* CellBank::getOrCreateUndoStack(int cellId)
{
    CellLayerStack* stack = mCellStacks.value(cellId, nullptr);
    if (!stack) {
        qDebug() << "[CellBank] getOrCreateUndoStack: no stack for cellId" << cellId;
        return nullptr;
    }
    return stack->activeUndoStack();
}

void CellBank::removeCellStack(int cellId)
{
    // No-op: undo stacks are owned by InnerLayer inside CellLayerStack.
    // They are freed when removeCell() deletes the CellLayerStack.
    Q_UNUSED(cellId)
}

// ── Inner Layer API (Task 3.2) ────────────────────────────────────────────────

BitmapImage* CellBank::getActiveInnerLayerImage(int cellId) const
{
    CellLayerStack* stack = mCellStacks.value(cellId, nullptr);
    if (!stack) return nullptr;
    InnerLayer* layer = stack->activeLayer();
    return layer ? layer->image : nullptr;
}

CellLayerStack* CellBank::getCellLayerStack(int cellId) const
{
    return mCellStacks.value(cellId, nullptr);
}

int CellBank::addInnerLayer(int cellId, const QString& name, int aboveIndex)
{
    CellLayerStack* stack = mCellStacks.value(cellId, nullptr);
    if (!stack) return -1;
    return stack->addLayer(name, aboveIndex);
}

void CellBank::removeInnerLayer(int cellId, int index)
{
    CellLayerStack* stack = mCellStacks.value(cellId, nullptr);
    if (!stack) return;
    stack->removeLayer(index);
}

void CellBank::setActiveInnerLayerIndex(int cellId, int index)
{
    CellLayerStack* stack = mCellStacks.value(cellId, nullptr);
    if (!stack) return;
    stack->setActiveIndex(index);
}

int CellBank::activeInnerLayerIndex(int cellId) const
{
    CellLayerStack* stack = mCellStacks.value(cellId, nullptr);
    if (!stack) return -1;
    return stack->activeIndex();
}

QUndoStack* CellBank::getActiveInnerLayerUndoStack(int cellId) const
{
    CellLayerStack* stack = mCellStacks.value(cellId, nullptr);
    if (!stack) return nullptr;
    return stack->activeUndoStack();
}

QUndoStack* CellBank::getInnerLayerUndoStack(int cellId, int innerLayerIndex) const
{
    CellLayerStack* stack = mCellStacks.value(cellId, nullptr);
    if (!stack) return nullptr;
    InnerLayer* layer = stack->layerAt(innerLayerIndex);
    return layer ? layer->undoStack : nullptr;
}

// ── Instance Tracking ─────────────────────────────────────────────────────────

void CellBank::addInstance(int cellId, int layerId, int framePos)
{
    if (!mCellStacks.contains(cellId)) return;

    CellInstance instance{layerId, framePos};
    if (!mCellInstances[cellId].contains(instance)) {
        mCellInstances[cellId].append(instance);
        qDebug() << "[CellBank] Added instance: Cell" << cellId << "@ layer" << layerId << "frame" << framePos;
    }
}

void CellBank::removeInstance(int cellId, int layerId, int framePos)
{
    if (!mCellInstances.contains(cellId)) return;

    CellInstance instance{layerId, framePos};
    mCellInstances[cellId].removeOne(instance);
    qDebug() << "[CellBank] Removed instance: Cell" << cellId << "@ layer" << layerId << "frame" << framePos;
}

void CellBank::moveInstance(int cellId, int layerId, int oldPos, int newPos)
{
    if (!mCellInstances.contains(cellId)) return;

    CellInstance oldInstance{layerId, oldPos};
    CellInstance newInstance{layerId, newPos};

    int index = mCellInstances[cellId].indexOf(oldInstance);
    if (index != -1) {
        mCellInstances[cellId][index] = newInstance;
        qDebug() << "[CellBank] Moved instance: Cell" << cellId << "@ layer" << layerId << "from" << oldPos << "to" << newPos;
    }
}

bool CellBank::isDisplayedAt(int cellId, int layerId, int framePos) const
{
    if (!mCellInstances.contains(cellId)) return false;

    CellInstance instance{layerId, framePos};
    return mCellInstances[cellId].contains(instance);
}

int CellBank::getCellIdAt(int layerId, int framePos) const
{
    for (auto it = mCellInstances.constBegin(); it != mCellInstances.constEnd(); ++it) {
        int cellId = it.key();
        CellInstance target{layerId, framePos};
        if (it.value().contains(target)) {
            return cellId;
        }
    }
    return -1;
}

void CellBank::rebuildInstanceTracking()
{
    qDebug() << "[CellBank] Rebuilding instance tracking...";
    mCellInstances.clear();
    qDebug() << "[CellBank] Instance tracking cleared, ready for rebuild";
}

void CellBank::setGlobalUndoLimit(int limit)
{
    if (limit < 0) limit = 0;
    sGlobalUndoLimit = limit;

    // Apply to all active undo stacks across all banks
    for (CellBank* bank : sAllBanks) {
        if (!bank) continue;
        for (auto it = bank->mCellStacks.constBegin(); it != bank->mCellStacks.constEnd(); ++it) {
            CellLayerStack* stack = it.value();
            if (!stack) continue;
            for (int i = 0; i < stack->layerCount(); ++i) {
                InnerLayer* layer = stack->layerAt(i);
                if (layer && layer->undoStack) {
                    layer->undoStack->clear();
                    layer->undoStack->setUndoLimit(sGlobalUndoLimit);
                }
            }
        }
    }
}

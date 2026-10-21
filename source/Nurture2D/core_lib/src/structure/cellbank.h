/*
Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef CELLBANK_H
#define CELLBANK_H

#include <QMap>
#include <QString>
#include <QObject>
#include <QUndoStack>
#include <QList>
#include <QLinkedList>
#include "pencilerror.h"
#include "celllayerstack.h"

class BitmapImage;
class QUndoStack;

// Tracks where a cell is used (frame position)
struct CellInstance {
    int layerId;
    int framePosition;
    
    bool operator==(const CellInstance& other) const {
        return layerId == other.layerId && framePosition == other.framePosition;
    }
};

class CellBank : public QObject
{
    Q_OBJECT
public:
    explicit CellBank(QObject* parent = nullptr);
    ~CellBank();

    // Core Management
    int addCell(BitmapImage* cell, const QString& name = QString());
    int addCellStack(CellLayerStack* stack, const QString& name, int cellId = -1);
    int addCellWithId(int cellId, BitmapImage* cell, const QString& name = QString());
    // Permanent deletion
    void removeCell(int cellId); 
    int duplicateCell(int id); // Creates a copy
    
    // Per-Cell Undo Stacks (delegate to CellLayerStack)
    QUndoStack* getOrCreateUndoStack(int cellId);
    void removeCellStack(int cellId);
    
    // Returns composite image — keeps all existing read-only callers unchanged
    BitmapImage* getCell(int id) const;
    QString getCellName(int id) const;
    void setCellName(int id, const QString& name);
    
    QList<int> getAllCellIds() const;
    int indexForCellId(int cellId) const;
    int cellIdForIndex(int index) const;
    void setCellOrder(const QList<int>& order);

    void touchCell(int cellId);
    static void setBitmapCacheLimitBytes(quint64 bytes);
    static quint64 bitmapCacheLimitBytes();

    // Naming Logic
    QString generateNextName(const QString& prefix);
    
    // Instance Tracking
    void addInstance(int cellId, int layerId, int framePos);
    void removeInstance(int cellId, int layerId, int framePos);
    void moveInstance(int cellId, int layerId, int oldPos, int newPos);
    bool isDisplayedAt(int cellId, int layerId, int framePos) const;
    int getCellIdAt(int layerId, int framePos) const;
    void rebuildInstanceTracking();

    // Inner Layer API (Task 3.2)
    BitmapImage*    getActiveInnerLayerImage(int cellId) const;
    CellLayerStack* getCellLayerStack(int cellId) const;
    int  addInnerLayer(int cellId, const QString& name, int aboveIndex = -1);
    void removeInnerLayer(int cellId, int index);
    void setActiveInnerLayerIndex(int cellId, int index);
    int  activeInnerLayerIndex(int cellId) const;
    QUndoStack* getActiveInnerLayerUndoStack(int cellId) const;
    QUndoStack* getInnerLayerUndoStack(int cellId, int innerLayerIndex) const;

    static void setGlobalUndoLimit(int limit);

signals:
    void cellAdded(int id);
    void cellRemoved(int id);
    void cellRenamed(int id, const QString& newName);

private:
    void trimBitmapCache(int protectedCellId);

    QMap<int, CellLayerStack*> mCellStacks;   // replaces mCells; owns all InnerLayers
    QMap<int, QString> mCellNames;
    QMap<int, QList<CellInstance>> mCellInstances;
    QList<int> mCellOrder;
    int mNextId = 1;

    QLinkedList<int> mLRU;
    QMap<int, QLinkedList<int>::iterator> mLRUIter;

    static quint64 sBitmapCacheLimitBytes;
    static int sGlobalUndoLimit;
    static QList<CellBank*> sAllBanks;
};

#endif // CELLBANK_H

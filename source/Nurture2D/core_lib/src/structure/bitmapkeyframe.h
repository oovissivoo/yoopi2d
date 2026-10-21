/*
Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.
*/

#ifndef BITMAPKEYFRAME_H
#define BITMAPKEYFRAME_H

#include "keyframe.h"
class QUndoStack; // forward decl
class CellBank;

class BitmapKeyFrame : public KeyFrame
{
public:
    explicit BitmapKeyFrame() : KeyFrame() {}
    explicit BitmapKeyFrame(int cellId) : KeyFrame(), mCellId(cellId) {}
    
    // Copy constructor should copy the reference only
    BitmapKeyFrame(const BitmapKeyFrame& other) : KeyFrame(other), mCellId(other.mCellId), mCellBankRef(other.mCellBankRef) {}

    KeyFrame* clone() const override { return new BitmapKeyFrame(*this); }
    
    int cellId() const { return mCellId; }
    void setCellId(int id) { mCellId = id; }
    void setCellBank(CellBank* bank) { mCellBankRef = bank; }
    CellBank* getCellBank() const { return mCellBankRef; }
    
    quint64 memoryUsage() override { return sizeof(int) + sizeof(void*); } // Lightweight reference

private:
    int mCellId = -1;
    CellBank* mCellBankRef = nullptr;
};

#endif // BITMAPKEYFRAME_H

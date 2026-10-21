/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef EYEDROPPERTOOL_H
#define EYEDROPPERTOOL_H

#include "basetool.h"

class LayerBitmap;
class LayerVector;

class EyedropperTool : public BaseTool
{
    Q_OBJECT
public:
    explicit EyedropperTool( QObject* parent = 0 );
    ToolType type() override { return EYEDROPPER; }
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;
    QCursor cursor( const QColor color );

    void pointerPressEvent( PointerEvent* ) override;
    void pointerReleaseEvent( PointerEvent* event ) override;
    void pointerMoveEvent( PointerEvent* event ) override;

    // Reference sampling mode: From View (composited) or Current Layer only
    void setReferenceMode(EyedropperReferenceMode mode) { properties.eyedropperReferenceMode = static_cast<int>(mode); markSettingsDirty(); }
    EyedropperReferenceMode referenceMode() const { return static_cast<EyedropperReferenceMode>(properties.eyedropperReferenceMode); }

    /** Updates front color for bitmap and color index for vector */
    void updateFrontColor(const QPointF& pos);

    // Sample-only mode: do not change front color; emit sampledColor instead
    void setSampleOnly(bool enabled) { mSampleOnly = enabled; }

    // Continuous pick mode: while enabled, pointerMoveEvent will continuously
    // update the front color (used by canvas Alt-hold picker only).
    void setContinuousPick(bool enabled) { mContinuousPick = enabled; }

signals:
    void sampledColor(const QColor& color);

private:
    /** Retrieves color of the pixel under the cursor for a bitmap layer */
    QColor getBitmapColor(LayerBitmap* layer, const QPointF& pos);
    /** Retrieves the color index of the pixel under the cursor for a vector layer */
    int getVectorColor(LayerVector *layer, const QPointF& pos);

    QColor sampleCompositedColor(const QPointF& viewportPos) const;

    bool mSampleOnly = false;
    bool mContinuousPick = false;
};

#endif // EYEDROPPERTOOL_H

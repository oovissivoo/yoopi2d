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

#ifndef HYBRIDCANVASSTACK_H
#define HYBRIDCANVASSTACK_H

#include <QRect>

class ScribbleArea;

// Facade that models the 3-level canvas stack (top preview, middle vector, bottom raster).
// Top preview layer: TiledBuffer (live stroke rendering during active drawing)
// Middle layer: Vector strokes
// Bottom raster layer: Committed bitmap content
// During active drawing, CanvasPainter composites the TiledBuffer tiles on top of
// the base bitmap layer to provide near-zero-latency stroke preview.
class HybridCanvasStack
{
public:
    explicit HybridCanvasStack(ScribbleArea* area);

    // Stroke lifecycle hooks
    void beginStroke(qreal brushWidth = 1.0);
    void updateStroke(const QRect& dirtyHint = QRect());
    void endStroke();

    // Re-render the raster bottom from vector middle for a region/frame
    void rerenderRasterFromVectors(int frame, const QRectF& canvasBoundsHint = QRectF());
    
    // Performance optimization: batch multiple rerender requests
    void requestRerenderBatch(int frame, const QRectF& bounds);
    void flushRerenderBatch();

private:
    ScribbleArea* mArea = nullptr;
    
    // Performance batching state
    struct PendingRerender {
        int frame = -1;
        QRectF bounds;
        bool valid = false;
    };
    PendingRerender mPendingRerender;
};

#endif // HYBRIDCANVASSTACK_H



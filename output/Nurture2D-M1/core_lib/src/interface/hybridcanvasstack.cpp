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

#include "hybridcanvasstack.h"

#include "scribblearea.h"
#include "editor.h"
#include "layervector.h"
#include "vectorimage.h"
#include "bitmapimage.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "colormanager.h"
#include "object.h"
#include "viewmanager.h"
#include <QPainter>
#include <QTimer>

HybridCanvasStack::HybridCanvasStack(ScribbleArea* area)
    : mArea(area)
{
}

void HybridCanvasStack::beginStroke(qreal brushWidth)
{
    if (!mArea) return;
    // Ensure top preview buffer is clean at stroke start
    mArea->clearDrawingBuffer();
    // Initialize batching for stroke rendering
    mArea->beginDrawingStroke(brushWidth);
    // Note: The TiledBuffer serves as the live preview (top) layer during active drawing.
    // CanvasPainter::paintCurrentBitmapFrame() will composite tiled buffer tiles on top
    // of the bitmap layer during ScribbleArea::paintEvent().
}


void HybridCanvasStack::updateStroke(const QRect& dirtyHint)
{
    // Trigger batched canvas update for live preview. Prefer using the provided
    // dirty hint (in screen coordinates) to compute a precise canvas-region to
    // update and then schedule a batched flush.
    if (!mArea) return;
    if (!dirtyHint.isEmpty()) {
        // Map screen rect to canvas space and use the returned rect
        QRectF canvasBounds = mArea->editor()->view()->mapScreenToCanvas(dirtyHint);
        mArea->updateCanvasRegion(canvasBounds);
    }
    mArea->scheduleCanvasUpdate();
}

void HybridCanvasStack::endStroke()
{
    // ScribbleArea::endStroke() handles the actual commit/clear and will disable
    // stroke-aware batching which triggers a final immediate flush.
}

void HybridCanvasStack::rerenderRasterFromVectors(int frame, const QRectF& /*canvasBoundsHint*/)
{
    if (!mArea) return;
    
    // Get current layer and check if it's vector
    auto* editor = mArea->editor();
    if (!editor) return;
    
    auto* layer = editor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::VECTOR) return;
    
    auto* vectorLayer = static_cast<LayerVector*>(layer);
    auto* vectorImage = vectorLayer->getLastVectorImageAtFrame(frame, 0);
    if (!vectorImage) return;
    
    // Find or create a corresponding bitmap layer for raster output
    // For now, we'll create a hidden bitmap layer that stores the rasterized version
    auto* layerManager = editor->layers();
    LayerBitmap* rasterLayer = nullptr;
    
    // Look for existing raster layer (could be named with suffix like "_raster")
    for (int i = 0; i < layerManager->count(); ++i) {
        auto* testLayer = layerManager->getLayer(i);
        if (testLayer->type() == Layer::BITMAP && testLayer->name().contains("_raster")) {
            rasterLayer = static_cast<LayerBitmap*>(testLayer);
            break;
        }
    }
    
    // Create raster layer if it doesn't exist
    if (!rasterLayer) {
        // For now, just invalidate frame - full layer creation will be implemented next
        mArea->updateFrame();
        return;
    }
    
    // Get or create bitmap image at current frame
    auto* bitmapImage = rasterLayer->getBitmapImageAtFrame(frame);
    if (!bitmapImage) {
        // Create new keyframe if needed
        rasterLayer->addNewKeyFrameAt(frame);
        bitmapImage = rasterLayer->getBitmapImageAtFrame(frame);
    }
    
    if (!bitmapImage) {
        mArea->updateFrame();
        return;
    }
    
    // Render vector strokes to bitmap
    QPainter painter(bitmapImage->image());
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Use the same rendering as VectorImage::paintImage but to bitmap
    vectorImage->paintImage(painter, *editor->object(), false, false, true);
    
    painter.end();
    
    // Mark the raster layer as modified
    rasterLayer->setModified(frame, true);
    
    // Invalidate frame to trigger repaint
    mArea->updateFrame();
}

void HybridCanvasStack::requestRerenderBatch(int frame, const QRectF& bounds)
{
    // Batch multiple rerender requests to avoid redundant work
    if (!mPendingRerender.valid || mPendingRerender.frame != frame) {
        mPendingRerender.frame = frame;
        mPendingRerender.bounds = bounds;
        mPendingRerender.valid = true;
    } else {
        // Expand bounds to include new request
        mPendingRerender.bounds = mPendingRerender.bounds.united(bounds);
    }
    
    // Use QTimer::singleShot for requestAnimationFrame-like behavior
    // This defers the actual rerender until the next event loop iteration
    QTimer::singleShot(0, [this]() {
        flushRerenderBatch();
    });
}

void HybridCanvasStack::flushRerenderBatch()
{
    if (!mPendingRerender.valid) return;
    
    // Perform the actual rerender with batched bounds
    rerenderRasterFromVectors(mPendingRerender.frame, mPendingRerender.bounds);
    
    // Clear pending state
    mPendingRerender.valid = false;
}



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

#include "canvaspainter.h"

#include <QtMath>
#include <QPainterPath>
#include <QDebug>

#include "object.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "bitmapimage.h"
#include "tile.h"
#include "tiledbuffer.h"
#include "vectorimage.h"
#include "managers/shifttracemanager.h"
#include "structure/bitmapkeyframe.h"
#include "structure/innerlayer.h"
#include "../../CUI/cosmicuimetrics.h"

#include "painterutils.h"
#include "graphics/vector/vectorimage.h"

CanvasPainter::CanvasPainter(QPixmap& canvas) : mCanvas(canvas)
{
    reset();
}

void CanvasPainter::paintShiftTraceGizmoOnCanvas(const QRect& viewport)
{
    // Draw on the main canvas pixmap so it appears above CameraPainter overlays
    if (!mShowShiftTracePivot || mShiftTraceManager == nullptr) return;
    if (mOnionSkinPainterOptions.isPlaying) return;
    if (mShiftTraceManager->hideAll()) return;

    QPainter painter;
    painter.begin(&mCanvas);
    // Optionally limit to viewport; but do not clip to paper/canvas content
    painter.setClipping(true);
    painter.setClipRect(viewport);
    paintShiftTraceGizmo(painter);
    painter.end();
}

CanvasPainter::~CanvasPainter()
{
}

void CanvasPainter::reset()
{
    mPostLayersPixmap = QPixmap(mCanvas.size());
    mPreLayersPixmap = QPixmap(mCanvas.size());
    mCurrentLayerPixmap = QPixmap(mCanvas.size());
    mOnionSkinPixmap = QPixmap(mCanvas.size());
    mShiftTracePixmap = QPixmap(mCanvas.size());
    mPreLayersPixmap.fill(Qt::transparent);
    mCanvas.fill(Qt::transparent);
    mCurrentLayerPixmap.fill(Qt::transparent);
    mPostLayersPixmap.fill(Qt::transparent);
    mOnionSkinPixmap.fill(Qt::transparent);
    mShiftTracePixmap.fill(Qt::transparent);
    mCurrentLayerPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mPreLayersPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mPostLayersPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mOnionSkinPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mShiftTracePixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
}

void CanvasPainter::setViewTransform(const QTransform view, const QTransform viewInverse)
{
    if (mViewTransform != view || mViewInverse != viewInverse) {
        mViewTransform = view;
        mViewInverse = viewInverse;
    }
}

void CanvasPainter::setTransformedSelection(QRect selection, QTransform transform)
{
    // Make sure that the selection is not empty
    if (selection.width() > 0 && selection.height() > 0)
    {
        mSelection = selection;
        mSelectionTransform = transform;
        mRenderTransform = true;
        mSelectionPath = QPainterPath();
    }
    else
    {
        // Otherwise we shouldn't be in transformation mode
        ignoreTransformedSelection();
    }
}

void CanvasPainter::setTransformedSelection(QRect selection, QTransform transform, const QPainterPath& selectionPath)
{
    setTransformedSelection(selection, transform);
    mSelectionPath = selectionPath;
}

void CanvasPainter::ignoreTransformedSelection()
{
    mRenderTransform = false;
}

void CanvasPainter::paintCached(const QRect& blitRect)
{
    if (!mPreLayersPixmapCacheValid)
    {
        QPainter preLayerPainter;
        initializePainter(preLayerPainter, mPreLayersPixmap, blitRect);
        renderPreLayers(preLayerPainter, blitRect);
        preLayerPainter.end();
        mPreLayersPixmapCacheValid = true;
    }

    QPainter mainPainter;
    initializePainter(mainPainter, mCanvas, blitRect);
    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPreLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);

    paintCurrentFrame(mainPainter, blitRect, mCurrentLayerIndex, mCurrentLayerIndex);

    if (!mPostLayersPixmapCacheValid)
    {
        QPainter postLayerPainter;
        initializePainter(postLayerPainter, mPostLayersPixmap, blitRect);
        renderPostLayers(postLayerPainter, blitRect);
        postLayerPainter.end();
        mPostLayersPixmapCacheValid = true;
    }

    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.setOpacity(1.0); // reset after current layer may have changed opacity
    mainPainter.drawPixmap(mPointZero, mPostLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);
}

void CanvasPainter::resetLayerCache()
{
    mPreLayersPixmapCacheValid = false;
    mPostLayersPixmapCacheValid = false;
}

void CanvasPainter::initializePainter(QPainter& painter, QPaintDevice& device, const QRect& blitRect)
{
    painter.begin(&device);

    // Only draw inside the clipped rectangle
    painter.setClipRect(blitRect);

    // Clear the area that's about to be painted again, to avoid painting on top of existing pixels
    // causing artifacts.
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(blitRect, Qt::transparent);

    // Surface has been cleared and is ready to be painted on
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setWorldMatrixEnabled(true);
    painter.setWorldTransform(mViewTransform);
}

void CanvasPainter::renderPreLayers(QPainter& painter, const QRect& blitRect)
{
    if (mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY || mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA)
    {
        paintCurrentFrame(painter, blitRect, 0, mCurrentLayerIndex - 1);
    }

    paintOnionSkin(painter, blitRect);
    // Integrate Shift/Trace as part of the canvas pass (z-ordered under current layer)
    paintShiftTrace(painter, blitRect);
    painter.setOpacity(1.0);
}

void CanvasPainter::renderPostLayers(QPainter& painter, const QRect& blitRect)
{
    if (mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY || mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA)
    {
        paintCurrentFrame(painter, blitRect, mCurrentLayerIndex + 1, mObject->getLayerCount() - 1);
    }
}

void CanvasPainter::setPaintSettings(const Object* object, int currentLayer, int frame, TiledBuffer* tiledBuffer)
{
    Q_ASSERT(object);
    mObject = object;

    CANVASPAINTER_LOG("Set CurrentLayerIndex = %d", currentLayer);
    mCurrentLayerIndex = currentLayer;
    mFrameNumber = frame;
    mTiledBuffer = tiledBuffer;
}

void CanvasPainter::paint(const QRect& blitRect)
{
    QPainter preLayerPainter;
    QPainter mainPainter;
    QPainter postLayerPainter;

    initializePainter(mainPainter, mCanvas, blitRect);

    initializePainter(preLayerPainter, mPreLayersPixmap, blitRect);
    renderPreLayers(preLayerPainter, blitRect);
    preLayerPainter.end();

    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPreLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);

    paintCurrentFrame(mainPainter, blitRect, mCurrentLayerIndex, mCurrentLayerIndex);

    initializePainter(postLayerPainter, mPostLayersPixmap, blitRect);
    renderPostLayers(postLayerPainter, blitRect);
    postLayerPainter.end();

    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.setOpacity(1.0); // reset after current layer may have changed opacity
    mainPainter.drawPixmap(mPointZero, mPostLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);

    // Draw Shift/Trace gizmo on top of all content
    paintShiftTraceGizmo(mainPainter);

    mPreLayersPixmapCacheValid = true;
    mPostLayersPixmapCacheValid = true;
}

void CanvasPainter::paintOnionSkinOnLayer(QPainter& painter, const QRect& blitRect, Layer* layer)
{
    mOnionSkinSubPainter.paint(painter, layer, mOnionSkinPainterOptions, mFrameNumber, [&] (OnionSkinPaintState state, int onionFrameNumber) {
        if (state == OnionSkinPaintState::PREV) {
            switch (layer->type())
            {
            case Layer::BITMAP: { paintBitmapOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizePrevFrames); break; }
            case Layer::VECTOR: { paintVectorOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizePrevFrames); break; }
            default: break;
            }
        }
        if (state == OnionSkinPaintState::NEXT) {
            switch (layer->type())
            {
            case Layer::BITMAP: { paintBitmapOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizeNextFrames); break; }
            case Layer::VECTOR: { paintVectorOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizeNextFrames); break; }
            default: break;
            }
        }
    });
}

void CanvasPainter::paintOnionSkin(QPainter& painter, const QRect& blitRect)
{
    // The slot system owns onion skins in the application. Rendering the legacy
    // pass as well leaves independent, often untinted ghosts behind the slots.
    // Keep that pass available for callers without a ShiftTraceManager.
    if (mShiftTraceManager) { return; }

    if (!mOptions.bOnionSkinMultiLayer || mOptions.eLayerVisibility == LayerVisibility::CURRENTONLY) {
        Layer* layer = mObject->getLayer(mCurrentLayerIndex);
        paintOnionSkinOnLayer(painter, blitRect, layer);
    } else {
        for (int i = 0; i < mObject->getLayerCount(); i++) {
            Layer* layer = mObject->getLayer(i);
            if (layer == nullptr) { continue; }

            paintOnionSkinOnLayer(painter, blitRect, layer);
        }
    }
}

void CanvasPainter::paintBitmapOnionSkinFrame(QPainter& painter, const QRect& blitRect, Layer* layer, int nFrame, bool colorize)
{
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);

    // Invariant (Req 6.3, 6.4): getBitmapImageAtFrame() → CellBank::getCell() →
    // CellLayerStack::composite(). Onion skin frames are rendered from the composite
    // of all visible inner layers, not from any individual inner layer. No code change
    // needed — the composite path is exercised automatically via getCell().
    BitmapImage* bitmapImage = bitmapLayer->getBitmapImageAtFrame(nFrame);

    if (bitmapImage == nullptr) { return; }
    bitmapImage->loadFile(); // Critical! force the BitmapImage to load the image

    QPainter onionSkinPainter;
    initializePainter(onionSkinPainter, mOnionSkinPixmap, blitRect);

    onionSkinPainter.drawImage(bitmapImage->topLeft(), *bitmapImage->image());
    paintOnionSkinFrame(painter, onionSkinPainter, nFrame, colorize, bitmapImage->getOpacity());
}

void CanvasPainter::paintVectorOnionSkinFrame(QPainter& painter, const QRect& blitRect, Layer* layer, int nFrame, bool colorize)
{
    LayerVector* vectorLayer = static_cast<LayerVector*>(layer);

    CANVASPAINTER_LOG("Paint Onion skin vector, Frame = %d", nFrame);
    VectorImage* vectorImage = vectorLayer->getVectorImageAtFrame(nFrame);
    if (vectorImage == nullptr) { return; }

    QPainter onionSkinPainter;
    initializePainter(onionSkinPainter, mOnionSkinPixmap, blitRect);

    vectorImage->paintImage(onionSkinPainter, *mObject, mOptions.bOutlines, mOptions.bThinLines, mOptions.bAntiAlias);
    paintOnionSkinFrame(painter, onionSkinPainter, nFrame, colorize, vectorImage->getOpacity());
}

void CanvasPainter::paintOnionSkinFrame(QPainter& painter, QPainter& onionSkinPainter, int nFrame, bool colorize, qreal frameOpacity)
{
    // Don't transform the image here as we used the viewTransform in the image output
    painter.setWorldMatrixEnabled(false);
    // Remember to adjust overall opacity based on opacity value from image
    painter.setOpacity(frameOpacity - (1.0-painter.opacity()));
    if (colorize)
    {
        QColor colorBrush = Qt::transparent; //no color for the current frame

        if (nFrame < mFrameNumber) { colorBrush = Qt::blue; }
        else if (nFrame > mFrameNumber) { colorBrush = Qt::red; }
        onionSkinPainter.setWorldMatrixEnabled(false);

        onionSkinPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        onionSkinPainter.setBrush(colorBrush);
        onionSkinPainter.drawRect(painter.viewport());
    }
    painter.drawPixmap(mPointZero, mOnionSkinPixmap);
}

void CanvasPainter::paintShiftTrace(QPainter& painter, const QRect& blitRect)
{
    if (mShiftTraceManager == nullptr) { return; }
    if (mOnionSkinPainterOptions.isPlaying) { return; }
    const auto& entries = mShiftTraceManager->entries();
    if (entries.isEmpty()) { return; }

    // Ensure we render as part of canvas space regardless of prior operations
    painter.save();
    painter.setWorldMatrixEnabled(true);
    painter.setWorldTransform(mViewTransform, false);

    bool prevBufActive = false;
    bool nextBufActive = false;
    QPainter prevBufPainter;
    QPainter nextBufPainter;
    QPixmap nextBuf;

    auto ensurePrevBuf = [&]() {
        if (prevBufActive) return;
        // Reuse persistent ShiftTrace pixmap as a scratch buffer (same size/DPR as canvas)
        mShiftTracePixmap.fill(Qt::transparent);
        initializePainter(prevBufPainter, mShiftTracePixmap, blitRect);
        prevBufActive = true;
    };
    auto ensureNextBuf = [&]() {
        if (nextBufActive) return;
        nextBuf = QPixmap(mCanvas.size());
        nextBuf.setDevicePixelRatio(mCanvas.devicePixelRatioF());
        nextBuf.fill(Qt::transparent);
        initializePainter(nextBufPainter, nextBuf, blitRect);
        nextBufActive = true;
    };

    int slotIndex = 0;
    for (const auto& e : entries) {
        const int currentSlot = slotIndex++;
        if (e.frameNumber < 1) continue;

        // Build entry transform — same for both held and live paths
        const qreal sVal = qIsFinite(e.scale) ? qMax<qreal>(0.01, e.scale) : 1.0;
        const qreal rVal = qIsFinite(e.rotationDeg) ? e.rotationDeg : 0.0;
        QTransform E;
        E.translate(e.pivot.x(), e.pivot.y());
        if (!qFuzzyIsNull(rVal)) E.rotate(rVal);
        if (!qFuzzyCompare(sVal, 1.0)) E.scale(sVal, sVal);
        E.translate(-e.pivot.x(), -e.pivot.y());
        E.translate(e.offset.x(), e.offset.y());

        // HoldGhost: render the isolated pixel copy using the same draw call as the live path.
        // Immune to any edits on the source frame after lock time.
        if (mShiftTraceManager->isSlotHeld(currentSlot)) {
            if (!mShiftTraceManager->hideAll() && e.visible) {
                const QImage& held = mShiftTraceManager->heldImage(currentSlot);
                if (!held.isNull()) {
                    const bool useTint = mShiftTraceManager->slotTintEnabled(currentSlot);
                    if (useTint) {
                        // Render into a scratch buffer, apply tint via SourceIn, composite onto canvas
                        QImage buf(held.size(), QImage::Format_ARGB32_Premultiplied);
                        buf.fill(Qt::transparent);
                        {
                            QPainter bp(&buf);
                            bp.drawImage(QPoint(0, 0), held);
                            bp.setCompositionMode(QPainter::CompositionMode_SourceIn);
                            bp.fillRect(buf.rect(), mShiftTraceManager->slotTintColor(currentSlot));
                        }
                        painter.save();
                        painter.setOpacity(qBound(0, e.opacityPercent, 100) / 100.0);
                        painter.setWorldTransform(E, true);
                        painter.drawImage(mShiftTraceManager->heldTopLeft(currentSlot), buf);
                        painter.restore();
                    } else {
                        painter.save();
                        painter.setOpacity(qBound(0, e.opacityPercent, 100) / 100.0);
                        painter.setWorldTransform(E, true);
                        painter.drawImage(mShiftTraceManager->heldTopLeft(currentSlot), held);
                        painter.restore();
                    }
                }
            }
            continue;
        }

        if (mShiftTraceManager->hideAll() || !e.visible) continue;

        Layer* layer = mObject->getLayer(e.layerIndex);
        if (!layer) continue;

        const bool isPrev = (e.frameNumber < mFrameNumber);
        const bool isNext = (e.frameNumber > mFrameNumber);
        const bool usePrevTint = isPrev && mShiftTraceManager->prevTintEnabled();
        const bool useNextTint = isNext && mShiftTraceManager->nextTintEnabled();

        QPainter* targetPainter = &painter;
        if (usePrevTint) {
            ensurePrevBuf();
            targetPainter = &prevBufPainter;
        } else if (useNextTint) {
            ensureNextBuf();
            targetPainter = &nextBufPainter;
        }

        targetPainter->save();
        targetPainter->setOpacity(qBound(0, e.opacityPercent, 100) / 100.0);

        switch (layer->type()) {
        case Layer::BITMAP: {
            LayerBitmap* bl = static_cast<LayerBitmap*>(layer);
            // Invariant (Req 6.4, 8.3): getBitmapImageAtFrame() → CellBank::getCell() →
            // CellLayerStack::composite(). Shift-trace reference frames use the composite
            // of all visible inner layers. No code change needed.
            BitmapImage* img = bl->getBitmapImageAtFrame(e.frameNumber);
            if (img) {
                img->loadFile();
                targetPainter->setWorldTransform(E, true);
                targetPainter->drawImage(img->topLeft(), *img->image());
            }
            break;
        }
        case Layer::VECTOR: {
            LayerVector* vl = static_cast<LayerVector*>(layer);
            VectorImage* vimg = vl->getVectorImageAtFrame(e.frameNumber);
            if (vimg) {
                targetPainter->setWorldTransform(E, true);
                vimg->paintImage(*targetPainter, *mObject, mOptions.bOutlines, mOptions.bThinLines, mOptions.bAntiAlias);
            }
            break;
        }
        default:
            break;
        }

        targetPainter->restore();
     }

    // Tint + composite buffers back onto the main canvas
    if (prevBufActive) {
        prevBufPainter.setWorldMatrixEnabled(false);
        prevBufPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        prevBufPainter.fillRect(blitRect, mShiftTraceManager->prevTintColor());
        prevBufPainter.end();

        painter.save();
        painter.setWorldMatrixEnabled(false);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setOpacity(1.0);
        painter.drawPixmap(mPointZero, mShiftTracePixmap);
        painter.restore();
    }
    if (nextBufActive) {
        nextBufPainter.setWorldMatrixEnabled(false);
        nextBufPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        nextBufPainter.fillRect(blitRect, mShiftTraceManager->nextTintColor());
        nextBufPainter.end();

        painter.save();
        painter.setWorldMatrixEnabled(false);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setOpacity(1.0);
        painter.drawPixmap(mPointZero, nextBuf);
        painter.restore();
    }
    painter.restore();
}

QRect CanvasPainter::computeShiftTraceEntryRenderBounds(const ShiftTraceManager::Entry& e) const
{
    // Determine canvas-space bounds of the frame for this entry
    QRectF canvasBounds;
    if (!mObject) return QRect();
    Layer* layer = mObject->getLayer(e.layerIndex);
    if (!layer) return QRect();

    if (layer->type() == Layer::BITMAP) {
        LayerBitmap* bl = static_cast<LayerBitmap*>(layer);
        canvasBounds = bl->getFrameBounds(e.frameNumber);
    } else if (layer->type() == Layer::VECTOR) {
        LayerVector* vl = static_cast<LayerVector*>(layer);
        VectorImage* vimg = vl->getVectorImageAtFrame(e.frameNumber);
        if (vimg) {
            QRectF r = vimg->getBoundsOfTransformedCurves();
            if (r.isEmpty()) r = QRectF(QPointF(0,0), vimg->getSize());
            canvasBounds = r;
        }
    }

    if (canvasBounds.isEmpty()) return QRect();

    // Build the same canvas-space entry transform used for rendering (including translation)
    const qreal sVal = qIsFinite(e.scale) ? qMax<qreal>(0.01, e.scale) : 1.0;
    const qreal rVal = qIsFinite(e.rotationDeg) ? e.rotationDeg : 0.0;
    QTransform E;
    E.translate(e.pivot.x(), e.pivot.y());
    if (!qFuzzyIsNull(rVal)) E.rotate(rVal);
    if (!qFuzzyCompare(sVal, 1.0)) E.scale(sVal, sVal);
    E.translate(-e.pivot.x(), -e.pivot.y());
    E.translate(e.offset.x(), e.offset.y());

    const QTransform combined = mViewTransform * E;
    QRectF devRectF = combined.mapRect(canvasBounds);

    // Add a small margin for antialiasing
    const qreal margin = 2.0;
    devRectF.adjust(-margin, -margin, margin, margin);

    // Clamp to the canvas/backbuffer bounds (device space)
    const qreal dpr = mCanvas.devicePixelRatioF();
    const QSize logicalCanvasSize(
        qMax(1, qRound(mCanvas.width() / dpr)),
        qMax(1, qRound(mCanvas.height() / dpr))
    );
    const QRect canvasRect(QPoint(0, 0), logicalCanvasSize);
    return devRectF.toAlignedRect().intersected(canvasRect);
}

void CanvasPainter::paintShiftTraceGizmo(QPainter& painter)
{
    if (!mShowShiftTracePivot || mShiftTraceManager == nullptr) return;
    if (mOnionSkinPainterOptions.isPlaying) return;
    const auto& entries = mShiftTraceManager->entries();
    if (entries.isEmpty()) return;
    if (mShiftTraceManager->hideAll()) return;

    const int activeId = mShiftTraceManager->activeMoveEntryId();
    if (activeId == -1) return;

    const ShiftTraceManager::Entry* ePtr = nullptr;
    for (const auto& e : entries) { if (e.id == activeId && e.visible) { ePtr = &e; break; } }
    if (!ePtr) return;
    const auto& e = *ePtr;

    painter.save();
    painter.setWorldMatrixEnabled(false);
    painter.setClipping(false);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor accentColor = CosmicColor::accent();
    QPen linePen(accentColor, 1.5);
    linePen.setCosmetic(true);
    QPen handlePen(accentColor, 1.5);
    handlePen.setCosmetic(true);

    // ── Build canvas-space transform, then map everything through mViewTransform ──
    // This is the only correct approach: build T in canvas space around the canvas
    // pivot, then let mViewTransform handle the canvas→screen mapping.
    const qreal s    = qMax<qreal>(0.01, qIsFinite(e.scale)       ? e.scale       : 1.0);
    const qreal rDeg = qIsFinite(e.rotationDeg) ? e.rotationDeg : 0.0;

    // Canvas-space transform: translate by offset, rotate/scale around pivot
    QTransform canvasT;
    canvasT.translate(e.pivot.x(), e.pivot.y());
    if (!qFuzzyIsNull(rDeg))    canvasT.rotate(rDeg);
    if (!qFuzzyCompare(s, 1.0)) canvasT.scale(s, s);
    canvasT.translate(-e.pivot.x(), -e.pivot.y());
    canvasT.translate(e.offset.x(), e.offset.y());

    // Combined: canvas transform → view transform
    QTransform fullT = canvasT * mViewTransform;

    // Gather bounding box corners in canvas space, map through fullT to screen
    QRectF canvasBounds;
    Layer* layer = mObject->getLayer(e.layerIndex);
    if (layer && layer->type() == Layer::BITMAP) {
        LayerBitmap* bl = static_cast<LayerBitmap*>(layer);
        canvasBounds = bl->getFrameBounds(e.frameNumber);
    } else if (layer && layer->type() == Layer::VECTOR) {
        LayerVector* vl = static_cast<LayerVector*>(layer);
        VectorImage* vimg = vl->getVectorImageAtFrame(e.frameNumber);
        if (vimg) {
            QRectF r = vimg->getBoundsOfTransformedCurves();
            if (r.isEmpty()) r = QRectF(QPointF(0, 0), vimg->getSize());
            canvasBounds = r;
        }
    }

    if (canvasBounds.isEmpty()) { painter.restore(); return; }

    // TL=0, TR=1, BR=2, BL=3
    QVector<QPointF> sc(4);
    sc[0] = fullT.map(canvasBounds.topLeft());
    sc[1] = fullT.map(canvasBounds.topRight());
    sc[2] = fullT.map(canvasBounds.bottomRight());
    sc[3] = fullT.map(canvasBounds.bottomLeft());

    // Move handle is always the center of the transformed bounding box
    const QPointF moveCenter = (sc[0] + sc[1] + sc[2] + sc[3]) * 0.25;

    // ── Bounding box edges ───────────────────────────────────────────────────
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(sc[0], sc[1]);
    painter.drawLine(sc[1], sc[2]);
    painter.drawLine(sc[2], sc[3]);
    painter.drawLine(sc[3], sc[0]);

    // ── Scale handles: outline squares at each corner ────────────────────────
    {
        const qreal half = 6.0;
        painter.setPen(handlePen);
        painter.setBrush(Qt::NoBrush);
        for (const QPointF& cs : sc) {
            painter.drawRect(QRectF(cs.x() - half, cs.y() - half, half * 2, half * 2));
        }
    }

    // ── Move handle: outline square at bbox center ───────────────────────────
    {
        const qreal half = 8.0;
        painter.setPen(handlePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(moveCenter.x() - half, moveCenter.y() - half, half * 2, half * 2));
    }

    // ── Rotation handle: outline square at midpoint of right edge ────────────
    {
        const qreal half = 6.0;
        const QPointF rotCenter = (sc[1] + sc[2]) * 0.5;
        painter.setPen(linePen);
        painter.drawLine(moveCenter, rotCenter);
        painter.setPen(handlePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(rotCenter.x() - half, rotCenter.y() - half, half * 2, half * 2));
    }

    painter.restore();
}

void CanvasPainter::paintFastPreview(const QRect& blitRect)
{
    QPainter painter;
    initializePainter(painter, mCanvas, blitRect);

    if (mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY || mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA)
    {
        paintCurrentFrame(painter, blitRect, 0, mCurrentLayerIndex - 1);
    }

    paintOnionSkin(painter, blitRect);
    paintShiftTrace(painter, blitRect);
    painter.setOpacity(1.0);

    paintCurrentFrame(painter, blitRect, mCurrentLayerIndex, mCurrentLayerIndex);

    if (mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY || mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA)
    {
        paintCurrentFrame(painter, blitRect, mCurrentLayerIndex + 1, mObject->getLayerCount() - 1);
    }

    painter.end();
}

void CanvasPainter::paintCurrentBitmapFrame(QPainter& painter, const QRect& blitRect, Layer* layer, bool isCurrentLayer)
{
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);

    BitmapImage* paintedImage = bitmapLayer->getLastBitmapImageAtFrame(mFrameNumber);

    if (paintedImage == nullptr) { return; }
    paintedImage->loadFile();

    const bool isDrawing = mTiledBuffer && !mTiledBuffer->bounds().isEmpty();

    QImage* img = paintedImage->image();

    // --- Layer bounds culling ---
    // Before touching mCurrentLayerPixmap (which costs a full-screen clear),
    // check if this layer's canvas-space bounds are even visible on screen.
    // With 25+ layers this skips the pixmap clear+blit for every off-screen layer.
    if (!isCurrentLayer || !isDrawing) {
        if (img && !img->isNull()) {
            const QRectF layerBoundsCanvas = QRectF(paintedImage->topLeft(), img->size());
            const QRectF layerBoundsScreen = mViewTransform.mapRect(layerBoundsCanvas);
            if (!layerBoundsScreen.intersects(QRectF(blitRect))) {
                return; // entirely off-screen, skip all pixmap work
            }
        }
    }

    QPainter currentBitmapPainter;
    initializePainter(currentBitmapPainter, mCurrentLayerPixmap, blitRect);
    if (!img || img->isNull()) {
        if (isCurrentLayer && isDrawing) {
            currentBitmapPainter.setCompositionMode(mOptions.cmBufferBlendMode);
            const auto tiles = mTiledBuffer->tiles();
            for (const Tile* tile : tiles) {
                const QRect tileCanvasRect(tile->pos(), tile->size());
                const QRect tileDeviceRect = mViewTransform.mapRect(QRectF(tileCanvasRect)).toAlignedRect();
                if (!blitRect.intersects(tileDeviceRect)) continue;
                currentBitmapPainter.drawPixmap(tile->posF(), tile->pixmap());
            }

            QColor liveTint;
            if (bitmapLayer->cellBank()) {
                KeyFrame* kf = bitmapLayer->getLastKeyFrameAtPosition(mFrameNumber);
                if (auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf)) {
                    CellLayerStack* st = bitmapLayer->cellBank()->getCellLayerStack(bkf->cellId());
                    if (st && st->activeLayer()) liveTint = st->activeLayer()->tintColor;
                }
            }
            if (liveTint.isValid()) {
                currentBitmapPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                liveTint.setAlphaF(1.0);
                currentBitmapPainter.fillRect(mTiledBuffer->bounds(), liveTint);
                currentBitmapPainter.setCompositionMode(mOptions.cmBufferBlendMode);
            }

            currentBitmapPainter.end();
            painter.save();
            painter.setWorldMatrixEnabled(false);
            painter.setTransform(QTransform());
            painter.drawPixmap(mPointZero, mCurrentLayerPixmap);
            painter.restore();
        }
        return;
    }

    // --- Viewport culling ---
    // Map the dirty screen rect back to canvas space and intersect with the
    // image bounds. This means at high zoom we only draw the small visible
    // sub-region of the image instead of the entire canvas, then let
    // mViewTransform scale it up. Cost becomes O(visible pixels) not O(canvas pixels).
    const QRectF visibleCanvasRectF = mViewInverse.mapRect(QRectF(blitRect));
    const QRectF imageBoundsF = QRectF(paintedImage->topLeft(), img->size());
    const QRectF drawRectF = visibleCanvasRectF.intersected(imageBoundsF);

    if (!drawRectF.isEmpty()) {
        // drawImage with source rect: only decodes/transforms the visible portion
        currentBitmapPainter.drawImage(drawRectF.topLeft(), *img,
                                       drawRectF.translated(-paintedImage->topLeft()));
    }

    if (isCurrentLayer && isDrawing)
    {
        currentBitmapPainter.setCompositionMode(mOptions.cmBufferBlendMode);
        const auto tiles = mTiledBuffer->tiles();
        for (const Tile* tile : tiles) {
            // Map tile bounds (canvas space) to device space for intersection with blitRect
            const QRect tileCanvasRect(tile->pos(), tile->size());
            const QRect tileDeviceRect = mViewTransform.mapRect(QRectF(tileCanvasRect)).toAlignedRect();
            if (!blitRect.intersects(tileDeviceRect)) continue;
            currentBitmapPainter.drawPixmap(tile->posF(), tile->pixmap());
        }

        // Apply paper tint to live stroke (same SourceIn technique as recomputeComposite)
        QColor liveTint;
        if (bitmapLayer->cellBank()) {
            KeyFrame* kf = bitmapLayer->getLastKeyFrameAtPosition(mFrameNumber);
            if (auto* bkf = dynamic_cast<BitmapKeyFrame*>(kf)) {
                CellLayerStack* st = bitmapLayer->cellBank()->getCellLayerStack(bkf->cellId());
                if (st && st->activeLayer()) liveTint = st->activeLayer()->tintColor;
            }
        }
        if (liveTint.isValid()) {
            currentBitmapPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            liveTint.setAlphaF(1.0);
            currentBitmapPainter.fillRect(mTiledBuffer->bounds(), liveTint);
            currentBitmapPainter.setCompositionMode(mOptions.cmBufferBlendMode);
        }
    }

    // Draw selection transformations on the current layer (also during pointer drags)
    if (isCurrentLayer && mRenderTransform) {
        paintTransformedSelection(currentBitmapPainter, paintedImage, mSelection, mSelectionPath);
    }

    // The current layer pixmap is rendered in device space (view transform was
    // applied while painting into mCurrentLayerPixmap). Blit it without the
    // world transform to avoid applying the view transform twice.
    painter.save();
    painter.setWorldMatrixEnabled(false);
    painter.setTransform(QTransform());
    // Combine layer opacity (already on painter) with per-frame opacity
    painter.setOpacity(painter.opacity() * paintedImage->getOpacity());
    painter.drawPixmap(mPointZero, mCurrentLayerPixmap);
    painter.restore();
}

void CanvasPainter::paintCurrentVectorFrame(QPainter& painter, const QRect& blitRect, Layer* layer, bool isCurrentLayer)
{
    LayerVector* vectorLayer = static_cast<LayerVector*>(layer);
    VectorImage* vectorImage = vectorLayer->getLastVectorImageAtFrame(mFrameNumber, 0);
    if (vectorImage == nullptr)
    {
        return;
    }

    QPainter currentVectorPainter;
    initializePainter(currentVectorPainter, mCurrentLayerPixmap, blitRect);

    const bool isDrawing = mTiledBuffer->isValid();

    if (mRenderTransform) {
        vectorImage->setSelectionTransformation(mSelectionTransform);
    }

    // Paint existing vector image to the painter
    vectorImage->paintImage(currentVectorPainter, *mObject, mOptions.bOutlines, mOptions.bThinLines, mOptions.bAntiAlias);

    if (isCurrentLayer && isDrawing) {
        currentVectorPainter.setCompositionMode(mOptions.cmBufferBlendMode);

        const auto tiles = mTiledBuffer->tiles();
        for (const Tile* tile : tiles) {
            const QRect tileCanvasRect(tile->pos(), tile->size());
            const QRect tileDeviceRect = mViewTransform.mapRect(QRectF(tileCanvasRect)).toAlignedRect();
            if (!blitRect.intersects(tileDeviceRect)) continue;
            currentVectorPainter.drawPixmap(tile->posF(), tile->pixmap());
        }
    }

    // Don't transform the image here as we used the viewTransform in the image output
    painter.setWorldMatrixEnabled(false);
    painter.setTransform(QTransform());

    // Combine the layer opacity (already set on painter) with the per-frame opacity
    painter.setOpacity(painter.opacity() * vectorImage->getOpacity());
    painter.drawPixmap(mPointZero, mCurrentLayerPixmap);
}

void CanvasPainter::paintTransformedSelection(QPainter& painter, BitmapImage* bitmapImage, const QRect& selection, const QPainterPath& selectionPath) const
{
    // Make sure there is something selected
    if (selection.width() == 0 && selection.height() == 0)
        return;

    QPixmap transformedPixmap = QPixmap(mSelection.size());
    transformedPixmap.fill(Qt::transparent);

    QPainter imagePainter(&transformedPixmap);
    imagePainter.translate(-selection.topLeft());
    if (!selectionPath.isEmpty()) {
        // Clip to the selection path for accurate masked preview (path is in canvas space)
        QPainterPath pathInLocal = selectionPath;
        pathInLocal.translate(-selection.topLeft());
        imagePainter.setClipPath(pathInLocal);
    }
    imagePainter.drawImage(bitmapImage->topLeft(), *bitmapImage->image());
    imagePainter.end();

    painter.save();

    // First, clear the original area in canvas space
    painter.setTransform(mViewTransform);
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    if (!selectionPath.isEmpty()) {
        painter.fillPath(selectionPath, QColor(255,255,255,255));
    } else {
        painter.fillRect(selection, QColor(255,255,255,255));
    }
    painter.restore();

    // Now draw the transformed selection using the combined transform
    QTransform combined = mSelectionTransform * mViewTransform;
    painter.setTransform(combined);

    if (!selectionPath.isEmpty()) {
        // Clip in painter's logical space by mapping the canvas path with the inverse combined transform
        painter.save();
        QTransform invCombined = combined.inverted();
        QPainterPath logicalClip = invCombined.map(selectionPath);
        painter.setClipPath(logicalClip);
        painter.drawPixmap(selection, transformedPixmap);
        painter.restore();
    } else {
        painter.drawPixmap(selection, transformedPixmap);
    }

    painter.restore();
}

/** Paints layers within the specified range for the current frame.
 *
 *  @param painter The painter to paint to
 *  @param startLayer The first layer to paint (inclusive)
 *  @param endLayer The last layer to paint (inclusive)
 */
void CanvasPainter::paintCurrentFrame(QPainter& painter, const QRect& blitRect, int startLayer, int endLayer)
{
    painter.setOpacity(1.0);

    bool isCameraLayer = mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA;

    for (int i = startLayer; i <= endLayer; ++i)
    {
        Layer* layer = mObject->getLayer(i);

        if (!layer->visible())
            continue;

        qreal baseOpacity = 1.0;
        if (!isCameraLayer)
        {
            if (mOptions.eLayerVisibility == LayerVisibility::RELATED && mOptions.bDimInactiveLayers)
            {
                baseOpacity = calculateRelativeOpacityForLayer(mCurrentLayerIndex, i, mOptions.fLayerVisibilityThreshold);
            }
            else if (mOptions.eLayerVisibility == LayerVisibility::ALL && mOptions.bDimInactiveLayers)
            {
                // Dim all non-current layers with a fixed opacity factor
                if (i != mCurrentLayerIndex)
                {
                    baseOpacity = mOptions.fDimInactiveOpacity;
                }
                else
                {
                    baseOpacity = 1.0;
                }
            }
            else
            {
                baseOpacity = 1.0;
            }
        }
        
        // Apply layer opacity
        painter.setOpacity(baseOpacity * layer->opacity());
        bool isCurrentLayer = mCurrentLayerIndex == i;

        CANVASPAINTER_LOG("  Render Layer[%d] %s", i, layer->name());
        switch (layer->type())
        {
        case Layer::BITMAP: { paintCurrentBitmapFrame(painter, blitRect, layer, isCurrentLayer); break; }
        case Layer::VECTOR: { paintCurrentVectorFrame(painter, blitRect, layer, isCurrentLayer); break; }
        default: break;
        }
    }
}

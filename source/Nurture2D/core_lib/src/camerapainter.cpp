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

#include "camerapainter.h"

#include <QPainter>
#include <QPixmap>
#include "object.h"
#include "layercamera.h"
#include "camera.h"
#include "keyframe.h"

#include "transform.h"

#include "painterutils.h"

void CameraPainter::setCanvasColorRgba(int rgba)
{
    const QColor c = QColor::fromRgba(rgba);
    if (!c.isValid()) return;
    if (mCanvasColor == c) return;
    mCanvasColor = c;
    resetCache();
}

void CameraPainter::setExtraSpaceSettings(int style, int scalePercent, int manualW, int manualH)
{
    style = (style == 1) ? 1 : 0;
    scalePercent = qBound(100, scalePercent, 200);

    const bool changed = (mExtraSpaceStyle != style)
                      || (mExtraSpaceScalePercent != scalePercent)
                      || (mExtraSpaceManualW != manualW)
                      || (mExtraSpaceManualH != manualH);

    mExtraSpaceStyle = style;
    mExtraSpaceScalePercent = scalePercent;
    mExtraSpaceManualW = manualW;
    mExtraSpaceManualH = manualH;

    if (changed) {
        resetCache();
    }
}

void CameraPainter::setPlaybackPresentationMode(bool enabled)
{
    if (mPlaybackPresentationMode == enabled) {
        return;
    }
    mPlaybackPresentationMode = enabled;
    resetCache();
}

CameraPainter::CameraPainter(QPixmap& canvas) : mCanvas(canvas)
{
    reset();
}

void CameraPainter::reset()
{
    mCameraPixmap = QPixmap(mCanvas.size());
    mCameraPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mCameraPixmap.fill(Qt::transparent);
}

void CameraPainter::resetCache()
{
    mCameraCacheValid = false;
}

void CameraPainter::preparePainter(const Object* object,
                                   int layerIndex,
                                   int frameIndex,
                                   const QTransform& transform,
                                   bool isPlaying,
                                   LayerVisibility layerVisibility,
                                   float relativeLayerOpacityThreshold,
                                   bool dimInactiveLayers,
                                   qreal viewScale)
{
    mObject = object;
    mCurrentLayerIndex = layerIndex;
    mFrameIndex = frameIndex;
    mViewTransform = transform;
    mIsPlaying = isPlaying;
    mLayerVisibility = layerVisibility;
    mRelativeLayerOpacityThreshold = relativeLayerOpacityThreshold;
    mDimInactiveLayers = dimInactiveLayers;
    mViewScale = viewScale;
}

void CameraPainter::paint(const QRect& blitRect)
{
    QPainter painter;
    initializePainter(painter, mCanvas, blitRect, false);
    paintVisuals(painter, blitRect);

    mCameraCacheValid = true;
}

void CameraPainter::paintCached(const QRect& blitRect)
{
    QPainter painter;
    // As always, initialize the painter with the canvas image, as this is what we'll paint on
    // In this case though because the canvas has already been painted, we're not interested in
    // having the blitter clear the image again, as that would remove our previous painted data, ie. strokes...
    initializePainter(painter, mCanvas, blitRect, false);
    if (!mCameraCacheValid) {
        paintVisuals(painter, blitRect);
        painter.end();
        mCameraCacheValid = true;
    } else {
        painter.setWorldMatrixEnabled(false);
        painter.drawPixmap(mZeroPoint, mCameraPixmap);
        painter.setWorldMatrixEnabled(true);
        painter.end();
    }
}

void CameraPainter::initializePainter(QPainter& painter, QPixmap& pixmap, const QRect& blitRect, bool blitEnabled)
{
    painter.begin(&pixmap);

    if (blitEnabled) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(blitRect, Qt::transparent);
        // Surface has been cleared and is ready to be painted on
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    painter.setClipRect(blitRect);
    painter.setWorldMatrixEnabled(true);
    painter.setWorldTransform(mViewTransform);
}

void CameraPainter::paintVisuals(QPainter& painter, const QRect& blitRect)
{
    LayerCamera* cameraLayerBelow = static_cast<LayerCamera*>(mObject->getLayerBelow(mCurrentLayerIndex, Layer::CAMERA));

    if (cameraLayerBelow == nullptr) { return; }

    const Layer* currentLayer = mObject->getLayer(mCurrentLayerIndex);

    if (mLayerVisibility == LayerVisibility::CURRENTONLY && currentLayer->type() != Layer::CAMERA) { return; }

    QPainter visualsPainter;
    initializePainter(visualsPainter, mCameraPixmap, blitRect, true);

    const bool presentationActive = (mPlaybackPresentationMode && mIsPlaying);

    if (!presentationActive && (!mIsPlaying || mOnionSkinOptions.enabledWhilePlaying)) {

        int startLayerI = 0;
        int endLayerI = mObject->getLayerCount() - 1;
        for (int i = startLayerI; i <= endLayerI; i++) {
            Layer* layer = mObject->getLayer(i);
            if (layer->type() != Layer::CAMERA) { continue; }

            LayerCamera* cameraLayer = static_cast<LayerCamera*>(layer);

            bool isCurrentLayer = cameraLayer == cameraLayerBelow;

            visualsPainter.save();
            visualsPainter.setOpacity(1);
            if (mLayerVisibility == LayerVisibility::RELATED && mDimInactiveLayers && !isCurrentLayer) {
                visualsPainter.setOpacity(calculateRelativeOpacityForLayer(mCurrentLayerIndex, i, mRelativeLayerOpacityThreshold));
            }

            paintOnionSkinning(visualsPainter, cameraLayer);

            visualsPainter.restore();
        }
    }

    if (!cameraLayerBelow->visible()) { return; }

    QTransform camTransform = cameraLayerBelow->getViewAtFrame(mFrameIndex);
    QRect cameraRect = cameraLayerBelow->getViewRect();

    if (presentationActive)
    {
        visualsPainter.save();
        QRect viewRect = visualsPainter.viewport();

        visualsPainter.setOpacity(1.0);
        visualsPainter.setWorldMatrixEnabled(true);
        visualsPainter.setPen(Qt::NoPen);
        visualsPainter.setBrush(Qt::black);
        visualsPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        const QTransform viewInverse = mViewTransform.inverted();
        const QRect boundingRect = viewInverse.mapRect(viewRect);

        QPolygon camPoly = camTransform.inverted().map(QPolygon(cameraRect));
        QPolygon boundingRectPoly = boundingRect;
        QPolygon visibleCanvasPoly = boundingRectPoly.subtracted(camPoly);
        visualsPainter.drawPolygon(visibleCanvasPoly);
        visualsPainter.restore();
    }
    else
    {
        paintBorder(visualsPainter, camTransform, cameraRect);
    }

    painter.setWorldMatrixEnabled(false);
    painter.drawPixmap(mZeroPoint, mCameraPixmap);
}

void CameraPainter::paintBorder(QPainter& painter, const QTransform& camTransform, const QRect& camRect)
{
    painter.save();
    QRect viewRect = painter.viewport();

    painter.setOpacity(1.0);
    painter.setWorldMatrixEnabled(true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#151515"));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    QTransform viewInverse = mViewTransform.inverted();
    QRect boundingRect = viewInverse.mapRect(viewRect);

    const int baseW = qMax(1, camRect.width());
    const int baseH = qMax(1, camRect.height());

    int effectiveW = baseW;
    int effectiveH = baseH;
    if (mExtraSpaceStyle == 1) {
        effectiveW = qBound(baseW, mExtraSpaceManualW, baseW * 2);
        effectiveH = qBound(baseH, mExtraSpaceManualH, baseH * 2);
    } else {
        const qreal s = qBound(1.0, static_cast<qreal>(mExtraSpaceScalePercent) / 100.0, 2.0);
        effectiveW = qBound(baseW, qRound(baseW * s), baseW * 2);
        effectiveH = qBound(baseH, qRound(baseH * s), baseH * 2);
    }

    const QPoint baseCenter = camRect.center();
    const QRect effectiveRect(baseCenter.x() - (effectiveW / 2),
                              baseCenter.y() - (effectiveH / 2),
                              effectiveW,
                              effectiveH);

    QPolygon camPoly = camTransform.inverted().map(QPolygon(effectiveRect));
    QPolygon boundingRectPoly = boundingRect;
    QPolygon visibleCanvasPoly = boundingRectPoly.subtracted(camPoly);

    painter.drawPolygon(visibleCanvasPoly);

    const qreal brightness = mCanvasColor.valueF();
    const QColor borderColor = (brightness >= 0.4) ? QColor(Qt::black) : QColor(Qt::white);
    QPen baseBorderPen(borderColor);
    baseBorderPen.setWidth(0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(baseBorderPen);
    QPolygon basePoly = camTransform.inverted().map(QPolygon(camRect));
    painter.drawPolygon(basePoly);

    painter.restore();
}

void CameraPainter::paintOnionSkinning(QPainter& painter, const LayerCamera* cameraLayer)
{
    QPen onionSkinPen;

    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setWorldMatrixEnabled(false);

    onionSkinPen.setStyle(Qt::PenStyle::DashLine);
    mOnionSkinPainter.paint(painter, cameraLayer, mOnionSkinOptions, mFrameIndex, [&] (OnionSkinPaintState state, int onionSkinNumber) {

        const QTransform& cameraTransform = cameraLayer->getViewAtFrame(onionSkinNumber);
        const QPolygonF& cameraPolygon = Transform::mapToWorldPolygon(cameraTransform, mViewTransform, cameraLayer->getViewRect());
        if (state == OnionSkinPaintState::PREV) {

            if (mOnionSkinOptions.colorizePrevFrames) {
                onionSkinPen.setColor(Qt::red);
            }

            painter.setPen(onionSkinPen);
            painter.drawPolygon(cameraPolygon);
        } else if (state == OnionSkinPaintState::NEXT) {
            if (mOnionSkinOptions.colorizeNextFrames) {
                onionSkinPen.setColor(Qt::blue);
            }

            painter.setPen(onionSkinPen);
            painter.drawPolygon(cameraPolygon);
        } else if (state == OnionSkinPaintState::CURRENT) {
            painter.save();
            painter.setPen(Qt::black);
            painter.drawPolygon(cameraPolygon);
            painter.restore();
        }
    });
    painter.restore();
}

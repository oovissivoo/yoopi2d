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
#include "clipboardmanager.h"

#include <QClipboard>
#include <QGuiApplication>

#include <editor.h>
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "cellbank.h"
#include "innerlayer.h"
#include <QDebug>
#include <QMimeData>
#include <QUuid>

ClipboardManager::ClipboardManager(Editor* editor) : BaseManager(editor, "ClipboardManager")
{
    resetStates();
}

ClipboardManager::~ClipboardManager()
{
    for (auto it : mFrames)
    {
        KeyFrame* frame = it.second;
        delete frame;
    }
}

void ClipboardManager::setFromSystemClipboard(const QPointF& pos, const Layer* layer)
{
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!mFrames.empty()) {
        const QMimeData* mime = clipboard->mimeData();
        if (mime && mime->data("application/x-yoopi2d-timeline-frames") == mFrameClipboardToken) return;
        resetStates();
    }
    if (!layer) return;

    // We intentially do not call resetStates here because we can only store image changes to the clipboard
    // otherwise we break pasting for vector.
    // Only bitmap is supported currently...
    // Only update clipboard data if it was stored by other applications
    if (layer->type() != Layer::BITMAP || clipboard->ownsClipboard()) {
        return;
    }

    QImage image = clipboard->image(QClipboard::Clipboard);
    if (!image.isNull()) {
        mBitmapImage = BitmapImage(pos.toPoint()-QPoint(image.size().width()/2, image.size().height()/2), image);
    }
}

void ClipboardManager::copyBitmapImage(BitmapImage* bitmapImage, QRectF selectionRect)
{
    resetStates();
    Q_ASSERT(bitmapImage != nullptr && bitmapImage->isLoaded());

    if (!selectionRect.isEmpty())
    {
        mBitmapImage = bitmapImage->copy(selectionRect.toRect());
    }
    else
    {
        mBitmapImage = bitmapImage->copy();
    }

    QGuiApplication::clipboard()->setImage(*mBitmapImage.image());
}

void ClipboardManager::copyVectorImage(const VectorImage* vectorImage)
{
    resetStates();
    if (vectorImage == nullptr || vectorImage->isEmpty()) { return; }

    // FIXME: handle vector selections, ie. independent strokes...
    mVectorImage = *vectorImage->clone();
}

void ClipboardManager::copySelectedFrames(const Layer* currentLayer)
{
    resetStates();

    for (int pos : currentLayer->selectedKeyFramesPositions()) {
        KeyFrame* keyframe = currentLayer->getKeyFrameAt(pos);
        Q_ASSERT(keyframe != nullptr);

        if (currentLayer->type() == Layer::BITMAP) {
            auto* layer = static_cast<const LayerBitmap*>(currentLayer);
            auto* bitmap = dynamic_cast<BitmapKeyFrame*>(keyframe);
            auto* stack = bitmap && layer->cellBank()
                ? layer->cellBank()->getCellLayerStack(bitmap->cellId()) : nullptr;
            if (!stack) {
                qWarning() << "[TimelineClipboard] Missing drawing at frame" << pos;
                continue;
            }
            for (int i = 0; i < stack->layerCount(); ++i) {
                if (stack->layerAt(i)->image) stack->layerAt(i)->image->loadFile();
            }
            mBitmapFrames[pos] = std::shared_ptr<const CellLayerStack>(stack->deepCopy());
        }

        KeyFrame* newKeyframe = keyframe->clone();
        // Unload unmodified keyframes now as they won't ever get unloaded
        // by activeframepool while in clipboard manager.
        newKeyframe->unloadFile();

        mFrames.insert(std::make_pair(keyframe->pos(), newKeyframe));
    }
    mFramesType = currentLayer->type();
    if (!mFrames.empty()) {
        mFrameClipboardToken = QUuid::createUuid().toByteArray();
        auto* mime = new QMimeData;
        mime->setData("application/x-yoopi2d-timeline-frames", mFrameClipboardToken);
        QGuiApplication::clipboard()->setMimeData(mime);
    }
    qInfo() << "[TimelineClipboard] Copied" << mFrames.size() << "drawing frames from layer" << currentLayer->id();
    emit editor()->canPasteChanged(editor()->canPaste());
}

std::shared_ptr<const CellLayerStack> ClipboardManager::bitmapFrameSnapshot(int position) const
{
    auto it = mBitmapFrames.find(position);
    return it == mBitmapFrames.end() ? nullptr : it->second;
}

std::map<int, KeyFrame*> ClipboardManager::getClipboardFrames()
{
    std::map<int, KeyFrame*> resultMap;
    for (auto it : mFrames)
    {
        resultMap.insert(std::make_pair(it.first, it.second->clone()));
    }
    return resultMap;
}

void ClipboardManager::resetStates()
{
    for (auto it : mFrames)
    {
        KeyFrame* frame = it.second;
        delete frame;
    }
    mFrames.clear();
    mBitmapFrames.clear();
    mFrameClipboardToken.clear();

    mBitmapImage = BitmapImage();
    mVectorImage = VectorImage();
    mFramesType = Layer::LAYER_TYPE::UNDEFINED;
}

void ClipboardManager::setBitmapFromImageAt(const QImage& image, const QPoint& topLeft)
{
    resetStates();
    if (image.isNull()) {
        return;
    }
    mBitmapImage = BitmapImage(topLeft, image);
    QGuiApplication::clipboard()->setImage(*mBitmapImage.image());
}

void ClipboardManager::setBitmapFromImageAtWithHybrid(const QImage& image, const QPoint& topLeft,
                                                      const QVector<BitmapImage::HybridVectorStroke>& strokes)
{
    resetStates();
    if (image.isNull()) {
        return;
    }
    mBitmapImage = BitmapImage(topLeft, image);
    for (const BitmapImage::HybridVectorStroke& stroke : strokes) {
        if (stroke.points.size() >= 2) {
            mBitmapImage.addHybridStroke(stroke.points, stroke.baseWidth);
        }
    }
    QGuiApplication::clipboard()->setImage(*mBitmapImage.image());
}

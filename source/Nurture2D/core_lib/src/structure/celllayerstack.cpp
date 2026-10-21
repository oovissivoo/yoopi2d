#include "celllayerstack.h"

#include "bitmapimage.h"

#include <QUndoStack>
#include <QPainter>
#include <QCoreApplication>

CellLayerStack::CellLayerStack()
    : mComposite(new BitmapImage)
{
    mLayers.append(new InnerLayer(QCoreApplication::translate("CellLayerStack", "Paper 1")));
}

CellLayerStack::CellLayerStack(BitmapImage* existingImage)
    : mComposite(new BitmapImage)
{
    mLayers.append(new InnerLayer(existingImage, QCoreApplication::translate("CellLayerStack", "Paper 1")));
}

CellLayerStack::~CellLayerStack()
{
    qDeleteAll(mLayers);
    mLayers.clear();
    delete mComposite;
}

CellLayerStack* CellLayerStack::deepCopy() const
{
    CellLayerStack* copy = new CellLayerStack();
    // Remove the default layer created by the constructor
    qDeleteAll(copy->mLayers);
    copy->mLayers.clear();

    for (const InnerLayer* src : mLayers) {
        InnerLayer* dst = new InnerLayer(src->name);
        dst->visible   = src->visible;
        dst->locked    = src->locked;
        dst->blendMode = src->blendMode;
        dst->setOpacity(src->opacity);
        dst->tintColor = src->tintColor;
        if (src->image) {
            delete dst->image;
            dst->image = new BitmapImage(*src->image);
        }
        copy->mLayers.append(dst);
    }

    copy->mActiveIndex    = mActiveIndex;
    copy->mCompositeDirty = true;
    return copy;
}

int CellLayerStack::addLayer(const QString& name, int aboveIndex)
{
    int insertAbove = (aboveIndex < 0) ? mActiveIndex : aboveIndex;
    insertAbove = qBound(0, insertAbove, mLayers.size() - 1);

    // Insert position is one above insertAbove
    int insertPos = insertAbove + 1;

    mLayers.insert(insertPos, new InnerLayer(name));

    // If inserted at or before active, shift active up
    if (insertPos <= mActiveIndex) {
        ++mActiveIndex;
    }

    // New layer becomes active
    mActiveIndex = insertPos;

    mCompositeDirty = true;
    return insertPos;
}

void CellLayerStack::removeLayer(int index)
{
    if (mLayers.size() <= 1) {
        return; // no-op: cannot remove the last layer
    }

    if (index < 0 || index >= mLayers.size()) {
        return;
    }

    delete mLayers.takeAt(index);

    // Adjust active index
    if (mActiveIndex >= mLayers.size()) {
        mActiveIndex = mLayers.size() - 1;
    } else if (index < mActiveIndex) {
        --mActiveIndex;
    }

    mCompositeDirty = true;
}

int CellLayerStack::layerCount() const
{
    return mLayers.size();
}

int CellLayerStack::activeIndex() const
{
    return mActiveIndex;
}

void CellLayerStack::setActiveIndex(int index)
{
    mActiveIndex = qBound(0, index, mLayers.size() - 1);
}

InnerLayer* CellLayerStack::activeLayer()
{
    if (mLayers.isEmpty()) return nullptr;
    mActiveIndex = qBound(0, mActiveIndex, mLayers.size() - 1);
    return mLayers[mActiveIndex];
}

const InnerLayer* CellLayerStack::activeLayer() const
{
    if (mLayers.isEmpty()) return nullptr;
    const int clamped = qBound(0, mActiveIndex, mLayers.size() - 1);
    return mLayers[clamped];
}

InnerLayer* CellLayerStack::layerAt(int index)
{
    if (index < 0 || index >= mLayers.size()) {
        return nullptr;
    }
    return mLayers[index];
}

const InnerLayer* CellLayerStack::layerAt(int index) const
{
    if (index < 0 || index >= mLayers.size()) {
        return nullptr;
    }
    return mLayers[index];
}

BitmapImage* CellLayerStack::composite()
{
    if (mCompositeDirty) {
        recomputeComposite();
    }
    return mComposite;
}

void CellLayerStack::invalidateComposite()
{
    mCompositeDirty = true;
}

void CellLayerStack::moveLayer(int fromIndex, int toIndex)
{
    if (fromIndex == toIndex) return;
    if (fromIndex < 0 || fromIndex >= mLayers.size()) return;
    if (toIndex   < 0 || toIndex   >= mLayers.size()) return;

    InnerLayer* moving = mLayers.takeAt(fromIndex);
    mLayers.insert(toIndex, moving);

    // Adjust active index to follow the moved layer
    if (mActiveIndex == fromIndex) {
        mActiveIndex = toIndex;
    } else if (fromIndex < mActiveIndex && toIndex >= mActiveIndex) {
        --mActiveIndex;
    } else if (fromIndex > mActiveIndex && toIndex <= mActiveIndex) {
        ++mActiveIndex;
    }
    mActiveIndex = qBound(0, mActiveIndex, mLayers.size() - 1);
    mCompositeDirty = true;
}

QUndoStack* CellLayerStack::activeUndoStack()
{
    InnerLayer* il = activeLayer();
    return il ? il->undoStack : nullptr;
}

void CellLayerStack::recomputeComposite()
{
    // Determine the union bounds of all visible layers
    QRect bounds;
    for (const InnerLayer* layer : mLayers) {
        if (!layer->visible || !layer->image) continue;
        layer->image->loadFile(); // ensure image data is loaded from disk
        QImage* img = layer->image->image();
        if (!img || img->isNull()) continue;
        QRect layerBounds(layer->image->bounds());
        if (bounds.isNull()) {
            bounds = layerBounds;
        } else {
            bounds = bounds.united(layerBounds);
        }
    }

    if (bounds.isNull()) {
        delete mComposite;
        mComposite = new BitmapImage();
        mCompositeDirty = false;
        return;
    }

    delete mComposite;
    mComposite = new BitmapImage(bounds, QColor(0, 0, 0, 0));

    QImage* compositeImg = mComposite->image();
    if (!compositeImg) {
        mCompositeDirty = false;
        return;
    }

    QPainter painter(compositeImg);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Flatten layers bottom-to-top
    for (const InnerLayer* layer : mLayers) {
        if (!layer->visible || !layer->image) continue;
        layer->image->loadFile(); // ensure image data is loaded from disk
        QImage* img = layer->image->image();
        if (!img || img->isNull()) continue;

        painter.setOpacity(layer->opacity);
        painter.setCompositionMode(blendModeToCompositionMode(layer->blendMode));

        QRect layerBounds = layer->image->bounds();
        QPoint destOffset = layerBounds.topLeft() - bounds.topLeft();

        if (layer->tintColor.isValid()) {
            // Non-destructive tint: draw pixels into a temp image, apply tint via SourceIn,
            // then composite the tinted result. Zero extra memory beyond one layer-sized image.
            QImage tinted(img->size(), QImage::Format_ARGB32_Premultiplied);
            tinted.fill(Qt::transparent);
            QPainter tp(&tinted);
            tp.drawImage(QPoint(0,0), *img);
            tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            QColor tc = layer->tintColor;
            tc.setAlphaF(1.0); // preserve original alpha shape, only replace color
            tp.fillRect(tinted.rect(), tc);
            tp.end();
            painter.drawImage(destOffset, tinted);
        } else {
            painter.drawImage(destOffset, *img);
        }
    }

    painter.end();
    mCompositeDirty = false;
}

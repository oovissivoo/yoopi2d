/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2018 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "tiledbuffer.h"

#include <QPainterPath>
#include <QtMath>
#include <QtConcurrent/QtConcurrent>

#include "tile.h"

TiledBuffer::TiledBuffer(QObject* parent) : QObject(parent)
{
    mBatchingEnabled = true;
    mImmediateFlush = false;
    mDeferredSignals = false;
    
    // Initialize spatial partitioning variables
    mRecentDabPositions.reserve(RECENT_DAB_HISTORY_SIZE);
    mRecentDabIndex = 0;
    mCurrentBrushWidth = 0.0;
}

TiledBuffer::~TiledBuffer()
{
    clear();
}

Tile* TiledBuffer::getTileFromIndex(const TileIndex& tileIndex, bool markDirty)
{
    Tile* selectedTile = mTiles.value(tileIndex, nullptr);

    if (!selectedTile) {
        // Time to allocate it, update table:
        const QPoint& tilePos (getTilePos(tileIndex));
        selectedTile = new Tile(tilePos, QSize(UNIFORM_TILE_SIZE, UNIFORM_TILE_SIZE));
        mTiles.insert(tileIndex, selectedTile);

        emit this->tileCreated(this, selectedTile);
    } else {
        if (markDirty) {
            if (mBatchingEnabled) {
                mDirtyTiles.insert(tileIndex);
                if (mDeferredSignals) {
                    mDeferredTileUpdates.insert(tileIndex);
                } else {
                    emit this->tileUpdated(this, selectedTile);
                }
            } else {
                if (mDeferredSignals) {
                    mDeferredTileUpdates.insert(tileIndex);
                } else {
                    emit this->tileUpdated(this, selectedTile);
                }
            }
        }
    }

    return selectedTile;
}

void TiledBuffer::setImmediateFlush(bool enabled)
{
    mImmediateFlush = enabled;
    if (enabled) {
        flushDirtyTiles();
    }
}

Tile* TiledBuffer::getTileFromIndexNoSignal(const TileIndex& tileIndex)
{
    Tile* selectedTile = mTiles.value(tileIndex, nullptr);
    if (!selectedTile) {
        const QPoint& tilePos (getTilePos(tileIndex));
        selectedTile = new Tile(tilePos, QSize(UNIFORM_TILE_SIZE, UNIFORM_TILE_SIZE));
        mTiles.insert(tileIndex, selectedTile);
        // no signals
    }
    return selectedTile;
}

void TiledBuffer::markTileUpdatedDeferred(const TileIndex& index)
{
    if (mImmediateFlush) {
        Tile* t = mTiles.value(index, nullptr);
        if (t) emit tileUpdated(this, t);
    } else {
        mDirtyTiles.insert(index);
    }
}

void TiledBuffer::markTileDirty(const TileIndex& index)
{
    if (mBatchingEnabled) {
        mDirtyTiles.insert(index);
        if (mDeferredSignals) {
            mDeferredTileUpdates.insert(index);
            return;
        }
    }
    if (!mDeferredSignals) {
        Tile* t = mTiles.value(index, nullptr);
        if (t) emit tileUpdated(this, t);
    }
}

void TiledBuffer::flushDirtyTiles()
{
    for (const TileIndex& idx : mDirtyTiles) {
        Tile* t = mTiles.value(idx, nullptr);
        if (t) emit tileUpdated(this, t);
    }
    mDirtyTiles.clear();
}

void TiledBuffer::setBatchingEnabled(bool enabled)
{
    mBatchingEnabled = enabled;
    if (!mBatchingEnabled) {
        flushDirtyTiles();
    }
}

void TiledBuffer::drawBrush(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing, bool useBatching) {
    // Check if we should use batching mode
    if (useBatching && mBatchingActive) {
        accumulateDab(point, brushWidth, pen, brush, cm, antialiasing);
        // For real-time visibility, render immediately to tiles
        // This provides immediate feedback while maintaining batching for performance
        renderDabImmediately(point, brushWidth, pen, brush, cm, antialiasing);
        return;
    }
    const float tileSize = UNIFORM_TILE_SIZE;

    // Gather the number of tiles that fits the size of the brush width
    const int xLeft = qFloor((qFloor(point.x() - brushWidth)) / tileSize);
    const int xRight = qFloor((qFloor(point.x() + brushWidth)) / tileSize);
    const int yTop = qFloor(qFloor(point.y() - brushWidth) / tileSize);
    const int yBottom = qFloor(qFloor(point.y() + brushWidth) / tileSize);

    // If we are not using antialiasing, make sure at least one pixel is within the brush's circle
    bool drawPoint = false;
    if (!antialiasing && brushWidth < 1.42) { // Overestimated approximation of 2*sqrt(2), which is the maximum distance a point can be from the center of a pixel
        // Measure the actual distance to the center of the nearest pixel
        const QPointF nearestPixelCenter(qRound(point.x()+0.5)-0.5, qRound(point.y()+0.5)-0.5);
        const qreal distanceToNearest = QLineF(point, nearestPixelCenter).length();
        if (distanceToNearest >= brushWidth/2) {
            // Nothing will be drawn with drawEllipse, so prepare to draw the nearest pixel with drawPoint
            drawPoint = true;
            point = QPointF(nearestPixelCenter.x() - 0.5, nearestPixelCenter.y() - 0.5);
            pen = QPen(brush, 1);
        }
    }
    const QRectF brushRect(point.x() - 0.5 * brushWidth, point.y() - 0.5 * brushWidth, brushWidth, brushWidth);

    for (int tileY = yTop; tileY <= yBottom; tileY++) {
        for (int tileX = xLeft; tileX <= xRight; tileX++) {

            Tile* tile = getTileFromIndex({tileX, tileY});

            QPainter painter(&tile->pixmap());

            painter.translate(-tile->pos());
            painter.setRenderHint(QPainter::Antialiasing, antialiasing);
            painter.setPen(pen);
            painter.setBrush(brush);
            painter.setCompositionMode(cm);
            if (drawPoint) {
                painter.drawPoint(point);
            } else {
                painter.drawEllipse(brushRect);
            }
            painter.end();

            mTileBounds.extend(tile->bounds());
        }
    }
}

void TiledBuffer::drawImage(const QImage& image, const QRect& imageBounds, QPainter::CompositionMode cm, bool antialiasing) {
    const float tileSize = UNIFORM_TILE_SIZE;

    // Use the actual image bounds — not inflated by image dimensions.
    // The old code used imageWidth/imageHeight as padding on both sides,
    // causing O(4x) tile visits for large brushes near tile boundaries.
    const int xLeft   = qFloor(imageBounds.left()   / tileSize);
    const int xRight  = qFloor((imageBounds.right()  - 1) / tileSize);
    const int yTop    = qFloor(imageBounds.top()    / tileSize);
    const int yBottom = qFloor((imageBounds.bottom() - 1) / tileSize);

    for (int tileY = yTop; tileY <= yBottom; tileY++) {
        for (int tileX = xLeft; tileX <= xRight; tileX++) {

            Tile* tile = getTileFromIndex({tileX, tileY});

            QPainter painter(&tile->pixmap());

            painter.translate(-tile->pos());
            painter.setRenderHint(QPainter::Antialiasing, antialiasing);
            painter.setCompositionMode(cm);
            painter.drawImage(imageBounds.topLeft(), image);
            painter.end();

            mTileBounds.extend(tile->bounds());
        }
    }
}


void TiledBuffer::drawPath(QPainterPath path, QPen pen, QBrush brush,
                           QPainter::CompositionMode cm, bool antialiasing)
{
    const qreal width = pen.widthF();
    const float tileSize = UNIFORM_TILE_SIZE;
    const QRectF pathRect = path.boundingRect();

    // Gather the number of tiles that fits the size of the brush width
    const int xLeft = qFloor((qFloor(pathRect.left() - width)) / tileSize);
    const int xRight = qFloor((qFloor(pathRect.right() + width)) / tileSize);
    const int yTop = qFloor(qFloor(pathRect.top() - width) / tileSize);
    const int yBottom = qFloor(qFloor(pathRect.bottom() + width) / tileSize);

    for (int tileY = yTop; tileY <= yBottom; tileY++) {
        for (int tileX = xLeft; tileX <= xRight; tileX++) {

            Tile* tile = getTileFromIndex({tileX, tileY});

            QPainter painter(&tile->pixmap());

            painter.translate(-tile->pos());
            painter.setRenderHint(QPainter::Antialiasing, antialiasing);
            painter.setPen(pen);
            painter.setBrush(brush);
            painter.setCompositionMode(cm);
            painter.drawPath(path);
            painter.end();

            mTileBounds.extend(tile->bounds());
        }
    }
}

void TiledBuffer::clear()
{
    QHashIterator<TileIndex, Tile*> i(mTiles);

    while (i.hasNext()) {
        i.next();
        Tile* tile = i.value();
        if (tile)
        {
            mTiles.remove(i.key());
            delete tile;
        }
    }

    mTileBounds = BlitRect();
    
    // Reset progressive commit state
    resetProgressiveCommitState();
}

QPoint TiledBuffer::getTilePos(const TileIndex& index) const
{
    return QPoint { qRound(UNIFORM_TILE_SIZE*static_cast<qreal>(index.x)),
                    qRound(UNIFORM_TILE_SIZE*static_cast<qreal>(index.y)) };
}

void TiledBuffer::setDeferredSignals(bool enabled)
{
    mDeferredSignals = enabled;
    if (!mDeferredSignals) {
        flushDeferredSignals();
    }
}

void TiledBuffer::flushDeferredSignals()
{
    for (const TileIndex& idx : mDeferredTileUpdates) {
        Tile* t = mTiles.value(idx, nullptr);
        if (t) emit tileUpdated(this, t);
    }
    mDeferredTileUpdates.clear();
}

void TiledBuffer::beginStroke(qreal initialBrushWidth)
{
    mBatchingActive = true;
    mPendingDabs.clear();
    setDeferredSignals(true);
    mDirtyTiles.clear();
    
    // Initialize progressive commit state
    mCurrentBrushWidth = initialBrushWidth;
    resetProgressiveCommitState();
}

void TiledBuffer::accumulateDab(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing)
{
    DabData dab;
    dab.point = point;
    dab.brushWidth = brushWidth;
    dab.pen = pen;
    dab.brush = brush;
    dab.cm = cm;
    dab.antialiasing = antialiasing;
    mPendingDabs.append(dab);
    
    // Track active region for progressive commits
    updateActiveRegion(point, brushWidth);
}

void TiledBuffer::flushDabs()
{
    if (mPendingDabs.isEmpty()) {
        mBatchingActive = false;
        // Restore normal signal behavior even if no dabs were recorded
        setDeferredSignals(false);
        return;
    }

    // Group dabs by affected tiles
    QHash<TileIndex, QVector<DabData>> dabsByTile;
    const float tileSize = UNIFORM_TILE_SIZE;

    for (const DabData& dab : mPendingDabs) {
        // Calculate affected tile range using same logic as drawBrush
        const int xLeft = qFloor((qFloor(dab.point.x() - dab.brushWidth)) / tileSize);
        const int xRight = qFloor((qFloor(dab.point.x() + dab.brushWidth)) / tileSize);
        const int yTop = qFloor(qFloor(dab.point.y() - dab.brushWidth) / tileSize);
        const int yBottom = qFloor(qFloor(dab.point.y() + dab.brushWidth) / tileSize);

        for (int tileY = yTop; tileY <= yBottom; tileY++) {
            for (int tileX = xLeft; tileX <= xRight; tileX++) {
                TileIndex tileIndex = {tileX, tileY};
                dabsByTile[tileIndex].append(dab);
            }
        }
    }

    // Track newly created tiles to emit tileCreated signals
    QVector<TileIndex> createdTiles;
    
    // Render all dabs for each tile in one QPainter session
    for (auto it = dabsByTile.begin(); it != dabsByTile.end(); ++it) {
        TileIndex tileIndex = it.key();
        
        // Check if tile was newly created before calling getTileFromIndexNoSignal
        bool wasNewlyCreated = !mTiles.contains(tileIndex);
        Tile* tile = getTileFromIndexNoSignal(tileIndex);
        
        if (wasNewlyCreated) {
            createdTiles.append(tileIndex);
        }
        
        renderDabsToTile(tile, it.value());
        markTileDirty(tileIndex);
        mTileBounds.extend(tile->bounds());
    }

    // Emit tileCreated signals for newly created tiles before tileUpdated signals
    for (const TileIndex& tileIndex : createdTiles) {
        Tile* tile = mTiles.value(tileIndex, nullptr);
        if (tile) {
            emit tileCreated(this, tile);
        }
    }

    flushDeferredSignals();
    // Disable deferral now that stroke is done
    setDeferredSignals(false);
    mPendingDabs.clear();
    mBatchingActive = false;
    
    // Reset progressive commit state at stroke end
    resetProgressiveCommitState();
}

void TiledBuffer::renderDabsToTile(Tile* tile, const QVector<DabData>& dabs)
{
    QPainter painter(&tile->pixmap());
    painter.translate(-tile->pos());

    for (const DabData& dab : dabs) {
        painter.setRenderHint(QPainter::Antialiasing, dab.antialiasing);
        painter.setPen(dab.pen);
        painter.setBrush(dab.brush);
        painter.setCompositionMode(dab.cm);

        // Handle special case for non-AA tiny brushes (same logic as drawBrush)
        bool drawPoint = false;
        QPointF point = dab.point;
        QPen pen = dab.pen;
        
        if (!dab.antialiasing && dab.brushWidth < 1.42) {
            const QPointF nearestPixelCenter(qRound(point.x()+0.5)-0.5, qRound(point.y()+0.5)-0.5);
            const qreal distanceToNearest = QLineF(point, nearestPixelCenter).length();
            if (distanceToNearest >= dab.brushWidth/2) {
                drawPoint = true;
                point = QPointF(nearestPixelCenter.x() - 0.5, nearestPixelCenter.y() - 0.5);
                pen = QPen(dab.brush, 1);
            }
        }

        const QRectF brushRect(point.x() - 0.5 * dab.brushWidth, point.y() - 0.5 * dab.brushWidth, dab.brushWidth, dab.brushWidth);

        if (drawPoint) {
            painter.drawPoint(point);
        } else {
            painter.drawEllipse(brushRect);
        }
    }

    painter.end();
}

void TiledBuffer::renderDabImmediately(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing)
{
    const float tileSize = UNIFORM_TILE_SIZE;

    // Gather the number of tiles that fits the size of the brush width
    const int xLeft = qFloor((qFloor(point.x() - brushWidth)) / tileSize);
    const int xRight = qFloor((qFloor(point.x() + brushWidth)) / tileSize);
    const int yTop = qFloor(qFloor(point.y() - brushWidth) / tileSize);
    const int yBottom = qFloor(qFloor(point.y() + brushWidth) / tileSize);

    // If we are not using antialiasing, make sure at least one pixel is within the brush's circle
    bool drawPoint = false;
    if (!antialiasing && brushWidth < 1.42) {
        const QPointF nearestPixelCenter(qRound(point.x()+0.5)-0.5, qRound(point.y()+0.5)-0.5);
        const qreal distanceToNearest = QLineF(point, nearestPixelCenter).length();
        if (distanceToNearest >= brushWidth/2) {
            drawPoint = true;
            point = QPointF(nearestPixelCenter.x() - 0.5, nearestPixelCenter.y() - 0.5);
            pen = QPen(brush, 1);
        }
    }
    const QRectF brushRect(point.x() - 0.5 * brushWidth, point.y() - 0.5 * brushWidth, brushWidth, brushWidth);

    for (int tileY = yTop; tileY <= yBottom; tileY++) {
        for (int tileX = xLeft; tileX <= xRight; tileX++) {
            Tile* tile = getTileFromIndex({tileX, tileY});
            QPainter painter(&tile->pixmap());
            painter.translate(-tile->pos());
            painter.setRenderHint(QPainter::Antialiasing, antialiasing);
            painter.setPen(pen);
            painter.setBrush(brush);
            painter.setCompositionMode(cm);
            if (drawPoint) {
                painter.drawPoint(point);
            } else {
                painter.drawEllipse(brushRect);
            }
            painter.end();
            mTileBounds.extend(tile->bounds());
        }
    }
}

// Spatial partitioning methods for progressive commits

void TiledBuffer::updateActiveRegion(QPointF dabPos, qreal brushWidth)
{
    // Store brush width for margin calculation
    mCurrentBrushWidth = brushWidth;
    
    // Add dab position to circular buffer
    if (mRecentDabPositions.size() < RECENT_DAB_HISTORY_SIZE) {
        mRecentDabPositions.append(dabPos);
    } else {
        mRecentDabPositions[mRecentDabIndex] = dabPos;
    }
    
    // Increment circular buffer index
    mRecentDabIndex = (mRecentDabIndex + 1) % RECENT_DAB_HISTORY_SIZE;
}

QRectF TiledBuffer::getActiveRegionBounds() const
{
    if (mRecentDabPositions.isEmpty()) {
        return QRectF();
    }
    
    // Find min/max coordinates from recent dab positions
    qreal minX = mRecentDabPositions[0].x();
    qreal maxX = mRecentDabPositions[0].x();
    qreal minY = mRecentDabPositions[0].y();
    qreal maxY = mRecentDabPositions[0].y();
    
    for (const QPointF& pos : mRecentDabPositions) {
        minX = qMin(minX, pos.x());
        maxX = qMax(maxX, pos.x());
        minY = qMin(minY, pos.y());
        maxY = qMax(maxY, pos.y());
    }
    
    // Calculate margin
    qreal margin = mCurrentBrushWidth * ACTIVE_REGION_MARGIN_MULTIPLIER;
    
    // Return expanded bounds
    return QRectF(minX - margin, minY - margin, 
                  maxX - minX + 2 * margin, maxY - minY + 2 * margin);
}

QRect TiledBuffer::getTileBounds(const TileIndex& index) const
{
    QPoint tilePos = getTilePos(index);
    return QRect(tilePos, QSize(UNIFORM_TILE_SIZE, UNIFORM_TILE_SIZE));
}

bool TiledBuffer::isTileInActiveRegion(const TileIndex& index) const
{
    QRectF activeRegion = getActiveRegionBounds();
    
    // If active region is empty, be conservative and keep all tiles
    if (activeRegion.isEmpty()) {
        return true;
    }
    
    QRect tileBounds = getTileBounds(index);
    return activeRegion.intersects(QRectF(tileBounds));
}

QVector<TileIndex> TiledBuffer::identifyCompletedTiles()
{
    QVector<TileIndex> completedTiles;
    
    for (auto it = mTiles.begin(); it != mTiles.end(); ++it) {
        const TileIndex& index = it.key();
        
        // Check if tile is not committed and not in active region
        if (!mCommittedTiles.contains(index) && !isTileInActiveRegion(index)) {
            completedTiles.append(index);
        }
    }
    
    return completedTiles;
}

void TiledBuffer::markTilesAsCommitted(const QVector<TileIndex>& indices)
{
    for (const TileIndex& index : indices) {
        mCommittedTiles.insert(index);
    }
}

void TiledBuffer::removeTiles(const QVector<TileIndex>& indices)
{
    for (const TileIndex& index : indices) {
        Tile* tile = mTiles.value(index, nullptr);
        if (tile) {
            delete tile;
            mTiles.remove(index);
            mDirtyTiles.remove(index);
            mDeferredTileUpdates.remove(index);
        }
    }
}

void TiledBuffer::resetProgressiveCommitState()
{
    mCommittedTiles.clear();
    mRecentDabPositions.clear();
    mRecentDabIndex = 0;
    mCurrentBrushWidth = 0.0;
}

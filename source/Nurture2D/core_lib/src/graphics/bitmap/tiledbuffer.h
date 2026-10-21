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
#ifndef TILEDBUFFER_H
#define TILEDBUFFER_H

#include <QPainter>
#include <QHash>
#include <QSet>

#include "blitrect.h"

class QImage;
class QRect;
class Tile;

struct TileIndex {
    int x;
    int y;
};

struct DabData {
    QPointF point;
    qreal brushWidth;
    QPen pen;
    QBrush brush;
    QPainter::CompositionMode cm;
    bool antialiasing;
};

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
inline uint qHash(const TileIndex &key, uint seed)
#else
inline size_t qHash(const TileIndex &key, size_t seed)
#endif
{
    return qHash(key.x, seed) ^ key.y;
}

inline bool operator==(const TileIndex &e1, const TileIndex &e2)
{
    return e1.x == e2.x
           && e1.y == e2.y;
}

class TiledBuffer: public QObject
{
    Q_OBJECT
public:
    TiledBuffer(QObject* parent = nullptr);
    ~TiledBuffer();

    /** Clears the content of the tiled buffer */
    void clear();

    /** Initializes batching mode for stroke rendering */
    void beginStroke(qreal initialBrushWidth = 1.0);
    /** Accumulates a dab for batched rendering */
    void accumulateDab(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing);
    /** Renders a dab immediately to tiles for real-time visibility */
    void renderDabImmediately(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing);
    /** Flushes all accumulated dabs to tiles */
    void flushDabs();

    /** Updates active region tracking with new dab position */
    void updateActiveRegion(QPointF dabPos, qreal brushWidth);
    /** Returns tiles outside active region that haven't been committed */
    QVector<TileIndex> identifyCompletedTiles();
    /** Marks tiles as committed to avoid re-committing */
    void markTilesAsCommitted(const QVector<TileIndex>& indices);
    /** Removes tiles from buffer after commit */
    void removeTiles(const QVector<TileIndex>& indices);
    /** Computes bounding box of recent dabs + margin */
    QRectF getActiveRegionBounds() const;
    /** Clears committed tiles set and dab history (called at stroke end) */
    void resetProgressiveCommitState();

    /** Returns true if there are any tiles, otherwise false */
    bool isValid() const { return !mTiles.isEmpty(); }

    /** Draws a brush with the specified parameters to the tiled buffer */
    void drawBrush(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing, bool useBatching = false);
    /** Draws a path with the specified parameters to the tiled buffer */
    void drawPath(QPainterPath path, QPen pen, QBrush brush,
                  QPainter::CompositionMode cm, bool antialiasing);
    /** Draws a image with the specified parameters to the tiled buffer */
    void drawImage(const QImage& image, const QRect& imageBounds, QPainter::CompositionMode cm, bool antialiasing);

    QHash<TileIndex, Tile*> tiles() const { return mTiles; }

    const QRect& bounds() const { return mTileBounds; }

signals:
    void tileUpdated(TiledBuffer* tiledBuffer, Tile* tile);
    void tileCreated(TiledBuffer* tiledBuffer, Tile* tile);

public:
    bool mImmediateFlush = false;
    void setImmediateFlush(bool enabled);
    Tile* getTileFromIndexNoSignal(const TileIndex& tileIndex);
    void markTileUpdatedDeferred(const TileIndex& index);

    inline QPoint getTilePos(const TileIndex& index) const;

    const int UNIFORM_TILE_SIZE = 64;

    BlitRect mTileBounds;

    QHash<TileIndex, Tile*> mTiles;

    // Batching for tile updates
    QSet<TileIndex> mDirtyTiles;
    bool mBatchingEnabled = true;

    void markTileDirty(const TileIndex& index);
    void flushDirtyTiles();
    void setBatchingEnabled(bool enabled);
    Tile* getTileFromIndex(const TileIndex& tileIndex, bool markDirty = true);
    // Deferred signals API
    void setDeferredSignals(bool enabled);
    void flushDeferredSignals();
    QVector<TileIndex> getDirtyTileIndices() const { return QVector<TileIndex>(mDirtyTiles.cbegin(), mDirtyTiles.cend()); }
private:
    bool mDeferredSignals = false;
    QSet<TileIndex> mDeferredTileUpdates;
    
    // Batching for stroke rendering
    QVector<DabData> mPendingDabs;
    bool mBatchingActive = false;
    
    // Spatial partitioning for progressive commits
    QVector<QPointF> mRecentDabPositions;  // circular buffer storing last 10 dab positions
    qreal mCurrentBrushWidth;             // current brush width for margin calculation
    int mRecentDabIndex;                  // circular buffer write index
    QSet<TileIndex> mCommittedTiles;     // tiles already committed during current stroke
    
    static const int RECENT_DAB_HISTORY_SIZE = 10;           // size of circular buffer
    static constexpr qreal ACTIVE_REGION_MARGIN_MULTIPLIER = 2.0; // safety margin multiplier
    
    /** Renders all dabs for a specific tile in one QPainter session */
    void renderDabsToTile(Tile* tile, const QVector<DabData>& dabs);
    
    // Helper methods for spatial partitioning
    bool isTileInActiveRegion(const TileIndex& index) const;
    QRect getTileBounds(const TileIndex& index) const;
};

#endif // TILEDBUFFER_H

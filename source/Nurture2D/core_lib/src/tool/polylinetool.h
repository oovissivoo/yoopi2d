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

#ifndef POLYLINETOOL_H
#define POLYLINETOOL_H

#include <atomic>
#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QPair>
#include <QPointF>
#include <QSet>
#include <QTimer>

#include "basetool.h"
#include "curvegeometrycache.h"

class PolylineTool : public BaseTool
{
    Q_OBJECT
public:
    explicit PolylineTool(QObject* parent = 0);
    ToolType type() override;
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;
    void resetToDefault() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent* event) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void pointerDoubleClickEvent(PointerEvent*) override;

    bool keyPressEvent(QKeyEvent* event) override;

    void paint(QPainter& painter, const QRect& blitRect) override;

    void clearToolData() override;
    bool leavingThisTool() override;
    bool isActive() const override;

private:
    QList<QPointF> mPoints;
    int mDragIndex = -1;
    int mHoverIndex = -1;
    QRect mLastOverlayRect;
    CurveGeometryCache mGeometryCache; // Cached precomputed segment geometry
    QPainterPath mPreviewPath;     // Vector preview path (centerline)
    bool mIsDragging = false;      // Active drag state - use lightweight preview
    QPainterPath mPreRenderedStroke; // Pre-computed final stroke for instant commit
    bool mStrokePreRendered = false; // Whether pre-rendered stroke is valid

    QTimer* mIdleBakeTimer = nullptr;

    QVector<QPainterPath> mPrevSegmentPaths;
    QVector<qreal> mPrevSegmentLengths;
    QVector<qreal> mSegmentStartDistances;
    qreal mLastRasterTotalLength = 0.0;
    int mLastRasterPointCount = 0;
    qreal mLastRasterWidth = 0.0;
    QRgb mLastRasterColor = 0;
    qreal mLastRasterTaperStart = 0.0;
    qreal mLastRasterTaperEnd = 0.0;
    QSet<int> mLastRasterStartTaperSegments;
    QSet<int> mLastRasterEndTaperSegments;

    QHash<quint64, QSet<int>> mTileToSegments;
    QVector<QVector<quint64>> mSegmentTiles;
    QVector<QVector<QPointF>> mSegmentDabCenters;
    QVector<QVector<qreal>> mSegmentDabWidths;
    QVector<QRectF> mSegmentDabBounds;

    QHash<quint64, QImage> mBakedTileImages;

    std::atomic<uint32_t> mBakeSerial{0};
    std::atomic<bool> mBakeInFlight{false};
    QMutex mBakeMutex;
    QSet<quint64> mPendingBakeTiles;
    QFutureWatcher<QVector<QPair<quint64, QImage>>>* mBakeWatcher = nullptr;

    void updatePreview(const QPointF* livePoint = nullptr);
    void commitStroke();
    void paintOverlay(QPainter& painter) override;
    int hitTestPoint(const QPointF& canvasPos) const;
    QRect computeOverlayDirtyRect() const;
    QPainterPath buildCachedPath(const QList<QPointF>& pts);

    void enqueueAsyncBakeTiles(const QSet<quint64>& tiles);
    void startAsyncBake();
    void bakeTilesSync(const QSet<quint64>& tiles);
    void stampBakedTilesToOverlay(const QSet<quint64>& tiles);
};

#endif // POLYLINETOOL_H

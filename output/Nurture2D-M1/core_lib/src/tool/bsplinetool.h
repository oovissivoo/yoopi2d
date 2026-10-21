#ifndef BSPLINETOOL_H
#define BSPLINETOOL_H

#include "basetool.h"
#include "curvegeometrycache.h"
#include <QPointF>
#include <QSet>
#include <QVector>
#include <QtGlobal>
#include <QHash>
#include <QFutureWatcher>
#include <QImage>
#include <QMutex>
#include <QPair>
#include <QTimer>
#include <atomic>

class BSplineTool : public BaseTool
{
    Q_OBJECT
public:
    explicit BSplineTool(QObject* parent = nullptr);

    ToolType type() override;
    void loadSettings() override;
    void saveSettings() override;
    QCursor cursor() override;
    void resetToDefault() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
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
    QRect mLastOverlayRect; // screen-space cached dirty rect for control points
    bool mClosed = false; // whether current stroke should be closed smoothly
    CurveGeometryCache mGeometryCache; // Cached precomputed segment geometry
    QPainterPath mPreviewPath;     // Vector preview path (centerline)
    bool mIsDragging = false;      // Active drag state - use lightweight preview
    int mDeferredRasterDirtyIndex = -1;
    bool mRasterPreviewSuppressed = false;
    QTimer* mIdleBakeTimer = nullptr;
    QHash<int, QPainterPath> mCachedSegmentStrokes; // Rendered segments for non-dirty parts
    QPainterPath mPreRenderedStroke; // Pre-computed final stroke path for instant commit
    bool mStrokePreRendered = false; // Whether pre-rendered stroke is valid
    QImage mPreRenderedRaster;       // Pre-rendered RASTER stroke (tapered) for instant blit
    QRect mPreRenderedRasterRect;    // Canvas rect of pre-rendered raster
    bool mRasterPreRendered = false; // Whether raster cache is valid

    QVector<QPainterPath> mPrevSegmentPaths;
    QVector<qreal> mPrevSegmentLengths;
    QVector<qreal> mSegmentStartDistances;
    qreal mLastRasterTotalLength = 0.0;
    int mLastRasterPointCount = 0;
    bool mLastRasterClosed = false;
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
    void enqueueAsyncBakeTiles(const QSet<quint64>& tiles);
    void startAsyncBake();
    void drawControlPoints();
    void commitStroke();
    void paintOverlay(QPainter& painter) override;
    int hitTestPoint(const QPointF& canvasPos) const;
    QRect computeOverlayDirtyRect() const; // screen-space union of control point bounds
    QPainterPath buildCachedPath(const QList<QPointF>& pts);
    void bakeTilesSync(const QSet<quint64>& tiles);
    void stampBakedTilesToOverlay(const QSet<quint64>& tiles);
};

#endif // BSPLINETOOL_H 
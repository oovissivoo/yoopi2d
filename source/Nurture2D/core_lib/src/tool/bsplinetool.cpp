#include "bsplinetool.h"

#include "managers/settingsmanager.h"
#include <QSettings>
#include <QPainterPath>
#include <QKeyEvent>

#include "../../../CUI/cosmicuimetrics.h"
#include "painterutils.h"
#include "editor.h"
#include "scribblearea.h"
#include "pointerevent.h"
#include "layermanager.h"
#include "colormanager.h"
#include "undoredomanager.h"
#include "layerbitmap.h"
#include "bitmapkeyframe.h"
#include "layervector.h"
#include "vectorimage.h"
#include "graphics/bitmap/bitmapimage.h"
#include "viewmanager.h"
#include "graphics/vector/beziercurve.h"
#include "toolmanager.h"
#include "penciltool.h"

// For overlay painting
#include <QPainter>

#include <QImage>

#include <QtConcurrent>
#include <QMutexLocker>

static inline qreal bmWidthAtDistance(qreal s, qreal totalLength, qreal fullWidth, qreal startTaperLen, qreal endTaperLen);

namespace {

static inline quint64 bmTileKey(int tx, int ty)
{
    const quint32 ux = quint32(qint32(tx));
    const quint32 uy = quint32(qint32(ty));
    return (quint64(ux) << 32) | quint64(uy);
}

static inline QRectF bmTileRectFromKey(quint64 k, int tileSize)
{
    const qint32 tx = qint32(quint32(k >> 32));
    const qint32 ty = qint32(quint32(k & 0xffffffffu));
    return QRectF(qreal(tx) * tileSize, qreal(ty) * tileSize, tileSize, tileSize);
}

static inline qreal bmDist2(const QPointF& a, const QPointF& b)
{
    const qreal dx = a.x() - b.x();
    const qreal dy = a.y() - b.y();
    return dx * dx + dy * dy;
}

static inline void bmDrawDabToPainter(QPainter& painter, QPointF point, qreal brushWidth, const QColor& color);

static void bmBuildSegmentDabs(const QPainterPath& segmentPath,
                              qreal segmentStartDistance,
                              qreal totalLength,
                              qreal fullWidth,
                              qreal startTaperLen,
                              qreal endTaperLen,
                              QVector<QPointF>& outCenters,
                              QVector<qreal>& outWidths,
                              QRectF& outBounds)
{
    outCenters.clear();
    outWidths.clear();
    outBounds = QRectF();

    const qreal segLen = segmentPath.length();
    if (segLen <= 0.0 || fullWidth <= 0.0) return;

    const qreal minStep = 0.5;
    qreal localDist = 0.0;

    while (localDist <= segLen) {
        const qreal s = segmentStartDistance + localDist;
        const qreal width = bmWidthAtDistance(s, totalLength, fullWidth, startTaperLen, endTaperLen);
        const qreal t = qBound<qreal>(0.0, segmentPath.percentAtLength(localDist), 1.0);
        const QPointF p = segmentPath.pointAtPercent(t);

        outCenters.push_back(p);
        outWidths.push_back(width);

        const qreal r = qMax<qreal>(0.5, width * 0.5);
        const QRectF dabRect(p.x() - r, p.y() - r, r * 2.0, r * 2.0);
        outBounds = outBounds.isEmpty() ? dabRect : outBounds.united(dabRect);

        const qreal step = qBound<qreal>(minStep, 0.25 * width, 0.45 * fullWidth);
        localDist += step;
    }
}

static void bmDrawCachedDabsToPainter(QPainter& painter,
                                     const QVector<QPointF>& centers,
                                     const QVector<qreal>& widths,
                                     const QColor& color,
                                     const QRectF& tileRect)
{
    const int count = qMin(centers.size(), widths.size());
    for (int i = 0; i < count; ++i) {
        const QPointF& p = centers[i];
        const qreal w = widths[i];
        const qreal r = qMax<qreal>(0.5, w * 0.5);
        if (p.x() + r < tileRect.left() || p.x() - r > tileRect.right() || p.y() + r < tileRect.top() || p.y() - r > tileRect.bottom()) {
            continue;
        }
        bmDrawDabToPainter(painter, p, w, color);
    }
}

static inline void bmDrawDabToPainter(QPainter& painter, QPointF point, qreal brushWidth, const QColor& color)
{
    bool drawPoint = false;
    if (brushWidth < 1.42) {
        const QPointF nearestPixelCenter(qRound(point.x() + 0.5) - 0.5, qRound(point.y() + 0.5) - 0.5);
        const qreal distanceToNearest = QLineF(point, nearestPixelCenter).length();
        if (distanceToNearest >= brushWidth / 2.0) {
            drawPoint = true;
            point = QPointF(nearestPixelCenter.x() - 0.5, nearestPixelCenter.y() - 0.5);
            brushWidth = 1.0;
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(color, Qt::SolidPattern));
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    if (drawPoint) {
        painter.drawPoint(point);
        return;
    }

    const QRectF brushRect(point.x() - 0.5 * brushWidth, point.y() - 0.5 * brushWidth, brushWidth, brushWidth);
    painter.drawEllipse(brushRect);
}

static void bmDrawDabsOnSegmentToPainter(QPainter& painter,
                                        const QPainterPath& segmentPath,
                                        const QColor& color,
                                        qreal fullWidth,
                                        qreal segmentStartDistance,
                                        qreal totalLength,
                                        qreal startTaperLen,
                                        qreal endTaperLen)
{
    const qreal segLen = segmentPath.length();
    if (segLen <= 0.0) return;

    const qreal minStep = 0.5;
    qreal localDist = 0.0;

    while (localDist <= segLen) {
        const qreal s = segmentStartDistance + localDist;
        const qreal width = bmWidthAtDistance(s, totalLength, fullWidth, startTaperLen, endTaperLen);
        const qreal t = qBound<qreal>(0.0, segmentPath.percentAtLength(localDist), 1.0);
        const QPointF p = segmentPath.pointAtPercent(t);
        bmDrawDabToPainter(painter, p, width, color);
        const qreal step = qBound<qreal>(minStep, 0.25 * width, 0.45 * fullWidth);
        localDist += step;
    }
}

static bool findHybridSnapCandidate(BitmapImage* bmp, const QPointF& canvasPos, qreal radius, QPointF& outPos)
{
    if (!bmp || radius <= 0.0) {
        return false;
    }

    const auto& strokes = bmp->hybridStrokes();
    if (strokes.isEmpty()) {
        return false;
    }

    const qreal radiusSq = radius * radius;
    qreal bestDistSq = radiusSq;
    bool found = false;

    for (const auto& stroke : strokes) {
        if (stroke.points.size() < 2 && stroke.intersectionPoints.isEmpty()) {
            continue;
        }

        QRectF expanded = stroke.bounds.adjusted(-radius, -radius, radius, radius);
        if (!expanded.contains(canvasPos)) {
            continue;
        }

        for (const QPointF& ip : stroke.intersectionPoints) {
            const qreal d2 = bmDist2(canvasPos, ip);
            if (d2 < bestDistSq) {
                bestDistSq = d2;
                outPos = ip;
                found = true;
            }
        }

        const auto& pts = stroke.points;
        for (int i = 0; i + 1 < pts.size(); ++i) {
            const QPointF& a = pts[i];
            const QPointF& b = pts[i + 1];
            const qreal abx = b.x() - a.x();
            const qreal aby = b.y() - a.y();
            const qreal abLenSq = abx * abx + aby * aby;
            if (abLenSq <= 0.0) {
                continue;
            }

            const qreal apx = canvasPos.x() - a.x();
            const qreal apy = canvasPos.y() - a.y();
            qreal t = (apx * abx + apy * aby) / abLenSq;
            if (t < 0.0) t = 0.0;
            else if (t > 1.0) t = 1.0;

            const QPointF proj(a.x() + abx * t, a.y() + aby * t);
            const qreal d2 = bmDist2(canvasPos, proj);
            if (d2 < bestDistSq) {
                bestDistSq = d2;
                outPos = proj;
                found = true;
            }
        }
    }

    return found;
}

static bool snapCanvasPosToHybrid(BitmapImage* bmp, const QPointF& canvasPos, int snapLevel, qreal viewScaleInv, QPointF& snapped)
{
    static const qreal snapPx[4] = { 0.0, 6.0, 10.0, 16.0 };
    const int level = qBound(0, snapLevel, 3);
    if (level <= 0) {
        return false;
    }
    const qreal radiusCanvas = snapPx[level] * viewScaleInv;
    return findHybridSnapCandidate(bmp, canvasPos, radiusCanvas, snapped);
}

// Sample actual points along a QPainterPath for accurate snap registration.
// Uses adaptive density: one sample per ~4px of path length, clamped to [8, 256].
static QVector<QPointF> samplePathPoints(const QPainterPath& path)
{
    const qreal len = path.length();
    if (len <= 0.0) return {};
    const int n = qBound(8, int(len / 4.0) + 1, 256);
    QVector<QPointF> pts;
    pts.reserve(n + 1);
    for (int i = 0; i <= n; ++i) {
        const qreal t = qreal(i) / qreal(n);
        pts.append(path.pointAtPercent(t));
    }
    return pts;
}

// Returns the active inner layer's BitmapImage for hybrid snap read/write.
// Must NOT use getBitmapImageAtFrame() — that returns the composite, which is
// deleted and recreated on every invalidateComposite() call, losing all stored
// hybrid strokes. The active inner layer image is stable across recomposites.
static BitmapImage* getSnapImage(LayerBitmap* bitmapLayer, int frameNumber)
{
    if (!bitmapLayer || !bitmapLayer->cellBank()) return nullptr;
    KeyFrame* kf = bitmapLayer->getLastKeyFrameAtPosition(frameNumber);
    if (!kf) return nullptr;
    BitmapKeyFrame* bkf = static_cast<BitmapKeyFrame*>(kf);
    return bitmapLayer->cellBank()->getActiveInnerLayerImage(bkf->cellId());
}

} // namespace

static inline bool bmHasTaper(const Properties& props)
{
    return (props.curveTaperStart > 0.0 || props.curveTaperEnd > 0.0);
}

static inline void bmComputeFixedPixelTaperLengths(qreal totalLength, qreal taperStartValue, qreal taperEndValue, qreal& outStartLen, qreal& outEndLen)
{
    outStartLen = 0.0;
    outEndLen = 0.0;

    const qreal requestedStart = qMax<qreal>(0.0, taperStartValue);
    const qreal requestedEnd = qMax<qreal>(0.0, taperEndValue);
    const qreal totalRequested = requestedStart + requestedEnd;

    if (totalRequested <= 0.0 || totalLength <= 0.0) {
        return;
    }

    if (totalLength >= totalRequested) {
        outStartLen = requestedStart;
        outEndLen = requestedEnd;
        return;
    }

    const qreal startRatio = (totalRequested > 0.0) ? (requestedStart / totalRequested) : 0.5;
    const qreal endRatio = 1.0 - startRatio;
    outStartLen = totalLength * startRatio;
    outEndLen = totalLength * endRatio;
}

static inline qreal bmWidthAtDistance(qreal s, qreal totalLength, qreal fullWidth, qreal startTaperLen, qreal endTaperLen)
{
    if (totalLength <= 0.0) {
        return fullWidth;
    }

    qreal startRamp = 1.0;
    if (startTaperLen > 0.0) {
        startRamp = qMin<qreal>(1.0, s / startTaperLen);
    }

    qreal endRamp = 1.0;
    if (endTaperLen > 0.0) {
        endRamp = qMin<qreal>(1.0, (totalLength - s) / endTaperLen);
    }

    const qreal ramp = qMin(startRamp, endRamp);
    qreal width = 1.0 + (fullWidth - 1.0) * ramp;
    if (width < 1.0) width = 1.0;
    return width;
}

static void bmDrawDabsOnSegment(ScribbleArea* scribble,
                               const QPainterPath& segmentPath,
                               const QColor& color,
                               qreal fullWidth,
                               qreal segmentStartDistance,
                               qreal totalLength,
                               qreal startTaperLen,
                               qreal endTaperLen)
{
    if (!scribble) return;
    const qreal segLen = segmentPath.length();
    if (segLen <= 0.0) return;

    const qreal minStep = 0.5;
    qreal localDist = 0.0;

    while (localDist <= segLen) {
        const qreal s = segmentStartDistance + localDist;
        const qreal width = bmWidthAtDistance(s, totalLength, fullWidth, startTaperLen, endTaperLen);
        const qreal t = qBound<qreal>(0.0, segmentPath.percentAtLength(localDist), 1.0);
        const QPointF p = segmentPath.pointAtPercent(t);
        scribble->drawPen(p, width, color, false);
        const qreal step = qBound<qreal>(minStep, 0.25 * width, 0.45 * fullWidth);
        localDist += step;
    }
}

static inline QPointF midpoint(const QPointF& a, const QPointF& b) {
    return (a + b) / 2.0;
}

BSplineTool::BSplineTool(QObject* parent) : BaseTool(parent)
{
    mBakeWatcher = new QFutureWatcher<QVector<QPair<quint64, QImage>>>(this);
    connect(mBakeWatcher, &QFutureWatcher<QVector<QPair<quint64, QImage>>>::finished, this, [this]() {
        if (!mBakeWatcher) return;
        const uint32_t jobSerial = mBakeWatcher->property("serial").toUInt();
        const uint32_t currentSerial = mBakeSerial.load(std::memory_order_relaxed);
        if (jobSerial != currentSerial) {
            mBakeInFlight.store(false, std::memory_order_relaxed);
            startAsyncBake();
            return;
        }

        const QVector<QPair<quint64, QImage>> results = mBakeWatcher->result();
        for (const auto& kv : results) {
            mBakedTileImages.insert(kv.first, kv.second);
        }

        mBakeInFlight.store(false, std::memory_order_relaxed);
        startAsyncBake();
    });

    mIdleBakeTimer = new QTimer(this);
    mIdleBakeTimer->setSingleShot(true);
    mIdleBakeTimer->setInterval(160);
    connect(mIdleBakeTimer, &QTimer::timeout, this, [this]() {
        if (!mEditor || !mEditor->layers()) return;
        Layer* layer = mEditor->layers()->currentLayer();
        if (!layer || layer->type() != Layer::BITMAP) return;
        if (mPoints.size() < 2) return;
        if (mDragIndex >= 0 || mIsDragging) return;
        // Cancel any stale in-flight bake results.
        {
            QMutexLocker lock(&mBakeMutex);
            mPendingBakeTiles.clear();
        }
        mBakeSerial.fetch_add(1, std::memory_order_relaxed);
        // Background bake only; do not stamp anything to the visible overlay.
        // Just enqueue all tiles for async baking so commit is fast.
        if (!mTileToSegments.isEmpty()) {
            QSet<quint64> all;
            all.reserve(mTileToSegments.size());
            for (auto it = mTileToSegments.begin(); it != mTileToSegments.end(); ++it) {
                all.insert(it.key());
            }
            enqueueAsyncBakeTiles(all);
        }
    });
}

void BSplineTool::bakeTilesSync(const QSet<quint64>& tiles)
{
    if (tiles.isEmpty()) return;
    if (mTileToSegments.isEmpty()) return;

    const QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                               ? mEditor->tools()->strokeColorOverride()
                               : mEditor->color()->frontColor();
    const int tileSize = 64;

    for (quint64 k : tiles) {
        const auto it = mTileToSegments.find(k);
        if (it == mTileToSegments.end()) continue;
        const QRectF tr = bmTileRectFromKey(k, tileSize);
        if (tr.isEmpty()) continue;

        QImage tileImg(tileSize, tileSize, QImage::Format_ARGB32_Premultiplied);
        tileImg.fill(Qt::transparent);

        QPainter tp(&tileImg);
        tp.setRenderHint(QPainter::Antialiasing, false);
        tp.setCompositionMode(QPainter::CompositionMode_Source);
        tp.translate(-tr.topLeft());
        tp.setClipRect(tr);

        const QSet<int>& segSet = it.value();
        for (int si : segSet) {
            if (si < 0 || si >= mSegmentDabBounds.size()) continue;
            if (si >= mSegmentDabCenters.size() || si >= mSegmentDabWidths.size()) continue;
            if (!mSegmentDabBounds[si].intersects(tr)) continue;
            bmDrawCachedDabsToPainter(tp, mSegmentDabCenters[si], mSegmentDabWidths[si], strokeColor, tr);
        }

        tp.end();
        mBakedTileImages.insert(k, tileImg);
    }
}

void BSplineTool::stampBakedTilesToOverlay(const QSet<quint64>& tiles)
{
    if (!mScribbleArea) return;
    if (tiles.isEmpty()) return;
    const int tileSize = 64;
    for (quint64 k : tiles) {
        const auto it = mBakedTileImages.find(k);
        if (it == mBakedTileImages.end()) continue;
        const QRectF tr = bmTileRectFromKey(k, tileSize);
        if (tr.isEmpty()) continue;
        mScribbleArea->drawImageOverlay(it.value(), tr, QPainter::CompositionMode_Source, false);
    }
}

void BSplineTool::enqueueAsyncBakeTiles(const QSet<quint64>& tiles)
{
    if (tiles.isEmpty()) return;
    {
        QMutexLocker lock(&mBakeMutex);
        for (quint64 k : tiles) mPendingBakeTiles.insert(k);
    }
    // Invalidate any in-flight bake results; this generation will be used by the next started job.
    mBakeSerial.fetch_add(1, std::memory_order_relaxed);
    startAsyncBake();
}

void BSplineTool::startAsyncBake()
{
    if (!mBakeWatcher || !mScribbleArea) return;
    if (mBakeInFlight.load(std::memory_order_relaxed)) return;

    QSet<quint64> batch;
    {
        QMutexLocker lock(&mBakeMutex);
        int count = 0;
        for (auto it = mPendingBakeTiles.begin(); it != mPendingBakeTiles.end() && count < 32; ) {
            batch.insert(*it);
            it = mPendingBakeTiles.erase(it);
            ++count;
        }
    }

    if (batch.isEmpty()) return;

    const uint32_t mySerial = mBakeSerial.load(std::memory_order_relaxed);
    mBakeInFlight.store(true, std::memory_order_relaxed);
    mBakeWatcher->setProperty("serial", QVariant(mySerial));

    const QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                               ? mEditor->tools()->strokeColorOverride()
                               : mEditor->color()->frontColor();
    const int tileSize = 64;

    struct SegmentDabs {
        QVector<QPointF> centers;
        QVector<qreal> widths;
        QRectF bounds;
    };

    QHash<quint64, QVector<int>> tileSegs;
    QSet<int> segUnion;
    tileSegs.reserve(batch.size());

    for (quint64 k : batch) {
        const auto it = mTileToSegments.find(k);
        if (it == mTileToSegments.end()) continue;
        QVector<int> segs;
        segs.reserve(it.value().size());
        for (int si : it.value()) {
            segs.push_back(si);
            segUnion.insert(si);
        }
        tileSegs.insert(k, segs);
    }

    QHash<int, SegmentDabs> segData;
    segData.reserve(segUnion.size());
    for (int si : segUnion) {
        if (si < 0 || si >= mSegmentDabCenters.size() || si >= mSegmentDabWidths.size() || si >= mSegmentDabBounds.size()) continue;
        SegmentDabs d;
        d.centers = mSegmentDabCenters[si];
        d.widths = mSegmentDabWidths[si];
        d.bounds = mSegmentDabBounds[si];
        segData.insert(si, d);
    }

    auto future = QtConcurrent::run([tileSize, strokeColor, batch, tileSegs, segData, mySerial, this]() -> QVector<QPair<quint64, QImage>> {
        QVector<QPair<quint64, QImage>> out;
        out.reserve(batch.size());

        for (quint64 k : batch) {
            if (mBakeSerial.load(std::memory_order_relaxed) != mySerial) break;
            const QRectF tr = bmTileRectFromKey(k, tileSize);
            if (tr.isEmpty()) continue;

            QImage tileImg(tileSize, tileSize, QImage::Format_ARGB32_Premultiplied);
            tileImg.fill(Qt::transparent);
            QPainter tp(&tileImg);
            tp.setRenderHint(QPainter::Antialiasing, false);
            tp.setCompositionMode(QPainter::CompositionMode_Source);
            tp.translate(-tr.topLeft());
            tp.setClipRect(tr);

            const auto it = tileSegs.find(k);
            if (it != tileSegs.end()) {
                for (int si : it.value()) {
                    const auto sdIt = segData.find(si);
                    if (sdIt == segData.end()) continue;
                    if (!sdIt.value().bounds.intersects(tr)) continue;
                    bmDrawCachedDabsToPainter(tp, sdIt.value().centers, sdIt.value().widths, strokeColor, tr);
                }
            }

            tp.end();
            out.push_back(qMakePair(k, tileImg));
        }
        return out;
    });

    mBakeWatcher->setFuture(future);
}

ToolType BSplineTool::type()
{
    return BSPLINE;
}

void BSplineTool::loadSettings()
{
    mPropertyEnabled[WIDTH] = true;
    mPropertyEnabled[ANTI_ALIASING] = true;
    mPropertyEnabled[DRAW_BEHIND] = true;
    mPropertyEnabled[CURVE_TAPER_START] = true;
    mPropertyEnabled[CURVE_TAPER_END] = true;
    mPropertyEnabled[HYBRID_SNAP_LEVEL] = true;

    SettingsManager* settings = SettingsManager::instance();
    properties.width = settings->getValue("Tool/BSpline/Width", settings->getValue("bsplineWidth", 8.0)).toDouble();
    properties.feather = -1;
    properties.pressure = false;
    properties.invisibility = OFF;
    properties.preserveAlpha = OFF;
    properties.useAA = settings->getValue("unifiedAA", 1).toInt();
    // Draw Behind is global; keep legacy read for migration but actual value comes from manager
    properties.drawBehind = settings->getValue("Tool/BSpline/DrawBehind", settings->getValue("bsplineDrawBehind", false)).toBool();
    
    // Load shared tapering settings (Curve and BSpline share these keys)
    // Fixed Pixels only: 0-200px range
    properties.curveTaperStart = settings->getValue("Tool/Curve/Shared/TaperStart", 0.0).toDouble();
    properties.curveTaperEnd = settings->getValue("Tool/Curve/Shared/TaperEnd", 0.0).toDouble();
    properties.hybridSnapLevel = settings->getValue("Tool/Curve/Shared/HybridSnapLevel", 0).toInt();
    
    // Clamp to fixed pixels range (0-200px)
    if (properties.curveTaperStart < 0.0) properties.curveTaperStart = 0.0;
    if (properties.curveTaperStart > 200.0) properties.curveTaperStart = 200.0;
    if (properties.curveTaperEnd < 0.0) properties.curveTaperEnd = 0.0;
    if (properties.curveTaperEnd > 200.0) properties.curveTaperEnd = 200.0;
    if (properties.hybridSnapLevel < 0) properties.hybridSnapLevel = 0;
    if (properties.hybridSnapLevel > 3) properties.hybridSnapLevel = 3;
    
    properties.stabilizerLevel = -1;
}

void BSplineTool::saveSettings()
{
    SettingsManager* settings = SettingsManager::instance();
    settings->deferredSetValue("Tool/BSpline/Width", properties.width);
    settings->deferredSetValue("unifiedAA", properties.useAA);
    // Draw Behind is global; do not persist per-tool anymore (compat no-op)
    settings->deferredSetValue("Tool/BSpline/DrawBehind", properties.drawBehind);
    // Save shared tapering settings (Curve and BSpline share these keys)
    settings->deferredSetValue("Tool/Curve/Shared/TaperStart", properties.curveTaperStart);
    settings->deferredSetValue("Tool/Curve/Shared/TaperEnd", properties.curveTaperEnd);
    settings->deferredSetValue("Tool/Curve/Shared/HybridSnapLevel", properties.hybridSnapLevel);
    clearSettingsDirty();
}

QCursor BSplineTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void BSplineTool::resetToDefault()
{
    setWidth(8.0);
}

void BSplineTool::clearToolData()
{
    if (mPoints.isEmpty() && mDragIndex < 0) { return; }
    mPoints.clear();
    mDragIndex = -1;
    mHoverIndex = -1;
    mClosed = false;
    mIsDragging = false;
    mCachedSegmentStrokes.clear();
    mGeometryCache.clear(); // Clear cached geometry
    mPreviewPath = QPainterPath();
    mPreRenderedStroke = QPainterPath();
    mStrokePreRendered = false;
    
    // Reset raster cache
    mPreRenderedRaster = QImage();
    mPreRenderedRasterRect = QRect();
    mRasterPreRendered = false;
    mPrevSegmentPaths.clear();
    mPrevSegmentLengths.clear();
    mSegmentStartDistances.clear();
    mLastRasterTotalLength = 0.0;
    mLastRasterPointCount = 0;
    mLastRasterClosed = false;
    mLastRasterWidth = 0.0;
    mLastRasterColor = 0;
    mLastRasterTaperStart = 0.0;
    mLastRasterTaperEnd = 0.0;
    mLastRasterStartTaperSegments.clear();
    mLastRasterEndTaperSegments.clear();

    if (mIdleBakeTimer) {
        mIdleBakeTimer->stop();
    }

    {
        QMutexLocker lock(&mBakeMutex);
        mPendingBakeTiles.clear();
    }
    mBakeSerial.fetch_add(1, std::memory_order_relaxed);
    mBakeInFlight.store(false, std::memory_order_relaxed);
    mTileToSegments.clear();
    mSegmentTiles.clear();
    mSegmentDabCenters.clear();
    mSegmentDabWidths.clear();
    mSegmentDabBounds.clear();
    mBakedTileImages.clear();
    if (mScribbleArea && !mLastOverlayRect.isEmpty()) {
        mScribbleArea->update(mLastOverlayRect.adjusted(-2, -2, 2, 2));
    }
    mLastOverlayRect = QRect();
    if (mScribbleArea) {
        mScribbleArea->clearDrawingBuffer();
        mScribbleArea->updateFrame();
    }
    emit isActiveChanged(BSPLINE, false);
}

bool BSplineTool::leavingThisTool()
{
    BaseTool::leavingThisTool();
    clearToolData();
    return true;
}

bool BSplineTool::isActive() const
{
    return !mPoints.isEmpty() || mDragIndex >= 0;
}

int BSplineTool::hitTestPoint(const QPointF& canvasPos) const
{
    if (mPoints.isEmpty()) return -1;
    // Larger tolerance in screen space for easier selection
    const qreal tolScreen = 12.0;
    const qreal tolCanvas = tolScreen * mEditor->view()->getScaleInversed();
    for (int i = 0; i < mPoints.size(); ++i) {
        if (QLineF(canvasPos, mPoints[i]).length() <= tolCanvas) {
            return i;
        }
    }
    return -1;
}

void BSplineTool::pointerPressEvent(PointerEvent* event)
{
    // Support RMB to remove the last point while creating
    const bool isRight = (event->button() == Qt::RightButton);
    if (isRight) {
        if (!mPoints.isEmpty()) {
            mPoints.removeLast();
            mDragIndex = -1;
            mHoverIndex = -1;
            if (mPoints.isEmpty()) {
                clearToolData();
            } else {
                mRasterPreviewSuppressed = true;
                if (mIdleBakeTimer) {
                    mIdleBakeTimer->stop();
                    mIdleBakeTimer->start();
                }
                mBakedTileImages.clear();
                updatePreview();
            }
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }

    // Ctrl+LMB: close+confirm if clicking the first point, otherwise plain confirm
    if (event->modifiers() & Qt::ControlModifier) {
        if (mPoints.size() >= 3 && hitTestPoint(event->canvasPos()) == 0) {
            mClosed = true;
            const qreal savedStart = properties.curveTaperStart;
            const qreal savedEnd   = properties.curveTaperEnd;
            properties.curveTaperStart = 0.0;
            properties.curveTaperEnd   = 0.0;
            updatePreview();
            commitStroke();
            properties.curveTaperStart = savedStart;
            properties.curveTaperEnd   = savedEnd;
        } else {
            commitStroke();
        }
        event->accept();
        return;
    }

    // Alt+LMB commits immediately
    if (event->modifiers() & Qt::AltModifier) {
        commitStroke();
        event->accept();
        return;
    }

    Layer* layer = mEditor->layers()->currentLayer();
    if (!(layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR)) {
        event->accept();
        return;
    }

    mScribbleArea->handleDrawingOnEmptyFrame();

    if (layer->type() == Layer::VECTOR) {
        VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame(), 0);
        Q_CHECK_PTR(vectorImage);
        vectorImage->deselectAll();
        if (mScribbleArea->makeInvisible() && !mEditor->preference()->isOn(SETTING::INVISIBLE_LINES)) {
            mScribbleArea->toggleThinLines();
        }
    }

    QPointF p = event->canvasPos();

    if (layer->type() == Layer::BITMAP && properties.hybridSnapLevel > 0) {
        auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
        BitmapImage* cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
        if (cellImg && mEditor && mEditor->view()) {
            QPointF snapped;
            if (snapCanvasPosToHybrid(cellImg, p, properties.hybridSnapLevel, mEditor->view()->getScaleInversed(), snapped)) {
                p = snapped;
            }
        }
    }
    int idx = hitTestPoint(p);
    // Ctrl+click on the first point closes the shape smoothly and commits
    if (idx == 0 && (event->modifiers().testFlag(Qt::ControlModifier)) && mPoints.size() >= 2) {
        mClosed = true;
        updatePreview();
        commitStroke();
        event->accept();
        return;
    }
    if (idx >= 0) {
        mDragIndex = idx;
        mIsDragging = true; // Enable lightweight preview
        mRasterPreviewSuppressed = true;
        if (mIdleBakeTimer) mIdleBakeTimer->stop();
        // Cancel any in-flight bakes; we do not want preview to wait.
        mBakeSerial.fetch_add(1, std::memory_order_relaxed);
        {
            QMutexLocker lock(&mBakeMutex);
            mPendingBakeTiles.clear();
        }
        mBakedTileImages.clear();
        // Mark nearby segments dirty for efficient recalculation
        mGeometryCache.markNearbySegmentsDirty(idx);
    } else {
        // Adding new point: mark last segment dirty (if any) and new segment
        const int lastIdx = mPoints.size() - 1;
        if (lastIdx >= 0) {
            mGeometryCache.markSegmentDirty(lastIdx);
        }
        mPoints << p;
        // Ensure cache is sized for new point count
        mGeometryCache.resize(mPoints.size());
        mGeometryCache.markSegmentDirty(mPoints.size() - 1);
        mRasterPreviewSuppressed = true;
        mBakedTileImages.clear();
        if (mIdleBakeTimer) {
            mIdleBakeTimer->stop();
            mIdleBakeTimer->start();
        }
        emit isActiveChanged(BSPLINE, true);
    }

    updatePreview();
    event->accept();
}

void BSplineTool::pointerMoveEvent(PointerEvent* event)
{
    QPointF p = event->canvasPos();

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer && layer->type() == Layer::BITMAP && properties.hybridSnapLevel > 0) {
        auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
        BitmapImage* cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
        if (cellImg && mEditor && mEditor->view()) {
            QPointF snapped;
            if (snapCanvasPosToHybrid(cellImg, p, properties.hybridSnapLevel, mEditor->view()->getScaleInversed(), snapped)) {
                p = snapped;
            }
        }
    }
    if (mDragIndex >= 0) {
        mPoints[mDragIndex] = p;
        // Mark nearby segments dirty when dragging a point
        mGeometryCache.markNearbySegmentsDirty(mDragIndex);
        // Invalidate raster cache - shape changed
        mRasterPreRendered = false;
        updatePreview();
    } else {
        const int newHover = hitTestPoint(p);
        if (newHover != mHoverIndex) {
            mHoverIndex = newHover;

            // Hovering should stay extremely cheap: only repaint control-point overlay.
            // Rebuilding the full cached path + tile/dab mapping on every mouse move
            // is a major performance regression.
            if (mScribbleArea) {
                QRect dirty = computeOverlayDirtyRect();
                if (!dirty.isEmpty()) {
                    QRect unionRect = dirty;
                    if (!mLastOverlayRect.isEmpty()) unionRect = unionRect.united(mLastOverlayRect);
                    unionRect.adjust(-3, -3, 3, 3);
                    mScribbleArea->update(unionRect);
                    mLastOverlayRect = dirty;
                } else {
                    mScribbleArea->update();
                }
            }
        }
    }
    event->accept();
}

void BSplineTool::pointerReleaseEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }
    if (mDragIndex >= 0) {
        mDragIndex = -1;
        mIsDragging = false; // Disable lightweight preview
        // Defer rasterization until the user is idle.
        mRasterPreviewSuppressed = true;
        if (mIdleBakeTimer) {
            mIdleBakeTimer->stop();
            mIdleBakeTimer->start();
        }
    }
    event->accept();
}

void BSplineTool::pointerDoubleClickEvent(PointerEvent* event)
{
    const bool isRight = (event->button() == Qt::RightButton);
    if (isRight) {
        // A rapid second RMB arrives as doubleClick instead of press — treat identically.
        if (!mPoints.isEmpty()) {
            mPoints.removeLast();
            mDragIndex = -1;
            mHoverIndex = -1;
            if (mPoints.isEmpty()) {
                clearToolData();
            } else {
                mRasterPreviewSuppressed = true;
                mBakedTileImages.clear();
                if (mIdleBakeTimer) { mIdleBakeTimer->stop(); mIdleBakeTimer->start(); }
                updatePreview();
            }
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        commitStroke();
        event->accept();
    }
}

bool BSplineTool::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Alt:
        {
            Layer* layer = mEditor->layers()->currentLayer();
            if (layer && layer->type() == Layer::BITMAP) {
                if (mEditor->tools()->beginTemporaryTool(EYEDROPPER, Qt::Key_Alt))
                {
                    return true;
                }
            }
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (mPoints.size() >= 2) {
            commitStroke();
            return true;
        }
        break;
    case Qt::Key_Backspace:
        if (!mPoints.isEmpty()) {
            mPoints.removeLast();
            if (mPoints.isEmpty()) {
                clearToolData();
                return true;
            }
            mRasterPreviewSuppressed = true;
            mBakedTileImages.clear();
            if (mIdleBakeTimer) {
                mIdleBakeTimer->stop();
                mIdleBakeTimer->start();
            }
            updatePreview();
            return true;
        }
        break;
    case Qt::Key_Escape:
        if (!mPoints.isEmpty() || mDragIndex >= 0) {
            clearToolData();
            return true;
        }
        break;
    default:
        break;
    }
    return BaseTool::keyPressEvent(event);
}

void BSplineTool::updatePreview(const QPointF* livePoint)
{
    const QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                               ? mEditor->tools()->strokeColorOverride()
                               : mEditor->color()->frontColor();
    QPen pen(strokeColor, properties.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    Layer* layer = mEditor->layers()->currentLayer();
    const bool isBitmapLayer = (layer && layer->type() == Layer::BITMAP);
    if (layer->type() == Layer::VECTOR) {
        if (mScribbleArea->makeInvisible()) {
            pen.setWidth(0);
            pen.setStyle(Qt::DotLine);
        } else {
            pen.setWidth(properties.width);
        }
    }

    QList<QPointF> pts = mPoints;
    if (livePoint) {
        // Use the current mouse position as a tentative last point during placement
        if (mDragIndex < 0) {
            pts << *livePoint;
            // Mark the new tentative segment dirty
            mGeometryCache.markSegmentDirty(pts.size() - 1);
        }
    }

    // Always keep a fast vector centerline preview path (used for both vector and bitmap layers).
    mPreviewPath = (pts.size() >= 2) ? buildCachedPath(pts) : QPainterPath();
    
    // Cache centerline path for fast path building on commit
    if (!mIsDragging && !livePoint && mPoints.size() >= 2) {
        mPreRenderedStroke = mPreviewPath;
        mStrokePreRendered = true;
    } else {
        mStrokePreRendered = false;
    }
    
    mRasterPreRendered = false;

    // Performance critical: when dragging control points on bitmap layers, only the centerline
    // preview needs to update in real-time. The full raster/tile/dab preparation is expensive
    // and should be deferred until idle (idle bake timer) or commit.
    if (isBitmapLayer && mIsDragging) {
        if (mScribbleArea) {
            mScribbleArea->update();
        }

        // Proactively update overlay region for crisp, artifact-free control points
        if (mScribbleArea) {
            QRect dirty = computeOverlayDirtyRect();
            if (!dirty.isEmpty()) {
                QRect unionRect = dirty;
                if (!mLastOverlayRect.isEmpty()) unionRect = unionRect.united(mLastOverlayRect);
                unionRect.adjust(-3, -3, 3, 3);
                mScribbleArea->update(unionRect);
                mLastOverlayRect = dirty;
            }
        }
        return;
    }

    // Bitmap mode: never stamp raster to the visible overlay during preview.
    // Background baking fills mBakedTileImages for fast commit.
    const bool allowRasterStamp = false;

    if (mScribbleArea && isBitmapLayer) {
        if (pts.size() < 2) {
            mScribbleArea->clearDrawingBuffer();
            mScribbleArea->updateFrame();
        } else {
            const int n = pts.size();
            const bool isClosed = mClosed;
            const int segCount = (isClosed ? n : ((n == 2) ? 1 : n));

            QVector<QPainterPath> newSegPaths;
            QVector<qreal> newStartDists;
            QVector<qreal> newSegLens;
            newSegPaths.reserve(segCount);
            newStartDists.reserve(segCount);
            newSegLens.reserve(segCount);

            if (segCount > 0) {
                qreal cum = 0.0;
                if (mGeometryCache.segmentCount() != n) {
                    mGeometryCache.resize(n);
                    mGeometryCache.markAllDirty();
                }
                if (!isClosed && n == 2) {
                    QPainterPath seg;
                    seg.moveTo(pts[0]);
                    seg.lineTo(pts[1]);
                    newStartDists.push_back(cum);
                    newSegPaths.push_back(seg);
                    const qreal len = QLineF(pts[0], pts[1]).length();
                    newSegLens.push_back(len);
                    cum += len;
                } else if (!isClosed && n >= 3) {
                    const QPointF m0 = midpoint(pts[0], pts[1]);
                    {
                        QPainterPath seg;
                        seg.moveTo(pts[0]);
                        seg.lineTo(m0);
                        newStartDists.push_back(cum);
                        newSegPaths.push_back(seg);
                        const qreal len = QLineF(pts[0], m0).length();
                        newSegLens.push_back(len);
                        cum += len;
                    }

                    for (int i = 1; i <= n - 2; ++i) {
                        if (mGeometryCache.isSegmentDirty(i)) {
                            CurveSegmentCache& seg = mGeometryCache.getSegment(i);
                            const QPointF miPrev = midpoint(pts[i - 1], pts[i]);
                            const QPointF mi = midpoint(pts[i], pts[i + 1]);
                            seg.segmentPath = QPainterPath();
                            seg.segmentPath.moveTo(miPrev);
                            seg.segmentPath.quadTo(pts[i], mi);
                            seg.length = seg.segmentPath.length();
                            seg.isDirty = false;
                        }
                        const CurveSegmentCache& cseg = mGeometryCache.getSegment(i);
                        newStartDists.push_back(cum);
                        newSegPaths.push_back(cseg.segmentPath);
                        newSegLens.push_back(cseg.length);
                        cum += cseg.length;
                    }

                    const QPointF mLast = midpoint(pts[n - 2], pts[n - 1]);
                    {
                        QPainterPath seg;
                        seg.moveTo(mLast);
                        seg.lineTo(pts[n - 1]);
                        newStartDists.push_back(cum);
                        newSegPaths.push_back(seg);
                        const qreal len = QLineF(mLast, pts[n - 1]).length();
                        newSegLens.push_back(len);
                        cum += len;
                    }
                } else if (isClosed && n >= 2) {
                    for (int i = 0; i < n; ++i) {
                        if (mGeometryCache.isSegmentDirty(i)) {
                            CurveSegmentCache& seg = mGeometryCache.getSegment(i);
                            if (i == 0) {
                                const QPointF startMid = midpoint(pts[0], pts[1]);
                                const QPointF miPrev = midpoint(pts[n - 1], pts[0]);
                                seg.segmentPath = QPainterPath();
                                seg.segmentPath.moveTo(miPrev);
                                seg.segmentPath.quadTo(pts[0], startMid);
                                seg.length = seg.segmentPath.length();
                            } else {
                                const int iPrev = (i - 1 + n) % n;
                                const int iNext = (i + 1) % n;
                                const QPointF miPrev = midpoint(pts[iPrev], pts[i]);
                                const QPointF mi = midpoint(pts[i], pts[iNext]);
                                seg.segmentPath = QPainterPath();
                                seg.segmentPath.moveTo(miPrev);
                                seg.segmentPath.quadTo(pts[i], mi);
                                seg.length = seg.segmentPath.length();
                            }
                            seg.isDirty = false;
                        }
                        const CurveSegmentCache& cseg = mGeometryCache.getSegment(i);
                        newStartDists.push_back(cum);
                        newSegPaths.push_back(cseg.segmentPath);
                        newSegLens.push_back(cseg.length);
                        cum += cseg.length;
                    }
                }

                const qreal totalLen = cum;
                qreal startTaperLen = 0.0;
                qreal endTaperLen = 0.0;
                if (bmHasTaper(properties)) {
                    bmComputeFixedPixelTaperLengths(totalLen, properties.curveTaperStart, properties.curveTaperEnd, startTaperLen, endTaperLen);
                }

                QSet<int> startTaperSegments;
                if (startTaperLen > 0.0) {
                    qreal acc = 0.0;
                    for (int si = 0; si < newSegLens.size() && acc < startTaperLen; ++si) {
                        startTaperSegments.insert(si);
                        acc += newSegLens[si];
                    }
                }

                const bool styleChanged = (mLastRasterWidth != properties.width)
                                          || (mLastRasterColor != strokeColor.rgba())
                                          || (mLastRasterTaperStart != properties.curveTaperStart)
                                          || (mLastRasterTaperEnd != properties.curveTaperEnd);

                const bool canIncremental = (!styleChanged)
                                            && (mLastRasterPointCount == n)
                                            && (mLastRasterClosed == isClosed)
                                            && (mPrevSegmentPaths.size() == segCount)
                                            && (mSegmentStartDistances.size() == segCount)
                                            && (mPrevSegmentLengths.size() == segCount);

                const bool tailPointCountChange = (!isClosed)
                                                 && (!styleChanged)
                                                 && (mLastRasterClosed == isClosed)
                                                 && (mLastRasterPointCount > 0)
                                                 && ((n == mLastRasterPointCount + 1) || (n == mLastRasterPointCount - 1));

                QSet<int> endTaperSegments;
                if (endTaperLen > 0.0 && totalLen > 0.0) {
                    qreal acc = 0.0;
                    for (int si = newSegLens.size() - 1; si >= 0 && acc < endTaperLen; --si) {
                        endTaperSegments.insert(si);
                        acc += newSegLens[si];
                    }
                }

                const int tileSize = 64;
                auto tileRect = [tileSize](quint64 k) -> QRectF {
                    const qint32 tx = qint32(quint32(k >> 32));
                    const qint32 ty = qint32(quint32(k & 0xffffffffu));
                    return QRectF(qreal(tx) * tileSize, qreal(ty) * tileSize, tileSize, tileSize);
                };
                auto tilesForRect = [tileSize](const QRectF& r) -> QVector<quint64> {
                    QVector<quint64> out;
                    if (r.isEmpty()) return out;
                    const int x0 = int(std::floor(r.left() / tileSize));
                    const int x1 = int(std::floor((r.right()) / tileSize));
                    const int y0 = int(std::floor(r.top() / tileSize));
                    const int y1 = int(std::floor((r.bottom()) / tileSize));
                    out.reserve((x1 - x0 + 1) * (y1 - y0 + 1));
                    for (int ty = y0; ty <= y1; ++ty) {
                        for (int tx = x0; tx <= x1; ++tx) {
                            out.push_back(bmTileKey(tx, ty));
                        }
                    }
                    return out;
                };
                auto removeSegFromTiles = [this](int si) {
                    if (si < 0 || si >= mSegmentTiles.size()) return;
                    const auto& keys = mSegmentTiles[si];
                    for (quint64 k : keys) {
                        auto it = mTileToSegments.find(k);
                        if (it == mTileToSegments.end()) continue;
                        it.value().remove(si);
                        if (it.value().isEmpty()) {
                            mTileToSegments.erase(it);
                        }
                    }
                    mSegmentTiles[si].clear();
                };
                auto addSegToTiles = [this](int si, const QVector<quint64>& keys) {
                    if (si < 0) return;
                    if (si >= mSegmentTiles.size()) mSegmentTiles.resize(si + 1);
                    mSegmentTiles[si] = keys;
                    for (quint64 k : keys) {
                        mTileToSegments[k].insert(si);
                    }
                };

                const qreal pad = properties.width * 2.0;
                const bool hasValidMap = (!mTileToSegments.isEmpty() && mSegmentTiles.size() == mPrevSegmentPaths.size());
                const bool canTileIncremental = canIncremental && hasValidMap;
                const bool canTailIncremental = tailPointCountChange && hasValidMap;

                if (!canTileIncremental && !canTailIncremental) {
                    mTileToSegments.clear();
                    mSegmentTiles.clear();
                    mSegmentTiles.resize(newSegPaths.size());
                    mSegmentDabCenters.clear();
                    mSegmentDabWidths.clear();
                    mSegmentDabBounds.clear();
                    mSegmentDabCenters.resize(newSegPaths.size());
                    mSegmentDabWidths.resize(newSegPaths.size());
                    mSegmentDabBounds.resize(newSegPaths.size());

                    for (int si = 0; si < newSegPaths.size(); ++si) {
                        const QRectF r = newSegPaths[si].boundingRect().adjusted(-pad, -pad, pad, pad);
                        addSegToTiles(si, tilesForRect(r));
                        bmBuildSegmentDabs(newSegPaths[si], newStartDists[si], totalLen, properties.width, startTaperLen, endTaperLen,
                                          mSegmentDabCenters[si], mSegmentDabWidths[si], mSegmentDabBounds[si]);
                    }

                    if (allowRasterStamp) {
                        const int asyncThresholdTiles = 48;
                        mScribbleArea->clearDrawingBuffer();
                        if (mTileToSegments.size() > asyncThresholdTiles) {
                            QSet<quint64> all;
                            all.reserve(mTileToSegments.size());
                            for (auto it = mTileToSegments.begin(); it != mTileToSegments.end(); ++it) {
                                all.insert(it.key());
                            }
                            enqueueAsyncBakeTiles(all);
                        } else {
                            for (auto it = mTileToSegments.begin(); it != mTileToSegments.end(); ++it) {
                                const quint64 k = it.key();
                                const QRectF tr = tileRect(k);
                                if (tr.isEmpty()) continue;
                                QImage tileImg(tileSize, tileSize, QImage::Format_ARGB32_Premultiplied);
                                tileImg.fill(Qt::transparent);
                                QPainter tp(&tileImg);
                                tp.setRenderHint(QPainter::Antialiasing, false);
                                tp.setCompositionMode(QPainter::CompositionMode_Source);
                                tp.translate(-tr.topLeft());
                                tp.setClipRect(tr);
                                const QSet<int>& segSet = it.value();
                                for (int si : segSet) {
                                    if (si < 0 || si >= newSegPaths.size()) continue;
                                    if (!mSegmentDabBounds[si].intersects(tr)) continue;
                                    bmDrawCachedDabsToPainter(tp, mSegmentDabCenters[si], mSegmentDabWidths[si], strokeColor, tr);
                                }
                                tp.end();
                                mScribbleArea->drawImageOverlay(tileImg, tr, QPainter::CompositionMode_Source, false);
                            }
                        }
                    }
                } else {
                    QSet<int> dirtySegs;

                    if (canTailIncremental) {
                        const int oldSegCount = mPrevSegmentPaths.size();
                        const int newSegCount = newSegPaths.size();

                        if (newSegCount < oldSegCount) {
                            for (int si = newSegCount; si < oldSegCount; ++si) {
                                removeSegFromTiles(si);
                            }
                            mSegmentTiles.resize(newSegCount);
                        } else if (newSegCount > oldSegCount) {
                            mSegmentTiles.resize(newSegCount);
                        }

                        const int startIdx = qMax(0, qMin(oldSegCount, newSegCount) - 4);
                        for (int si = startIdx; si < qMin(oldSegCount, newSegCount); ++si) dirtySegs.insert(si);
                        for (int si = qMin(oldSegCount, newSegCount); si < qMax(oldSegCount, newSegCount); ++si) dirtySegs.insert(si);
                    } else if (mDragIndex >= 0) {
                        const int idx = mDragIndex;
                        if (isClosed) {
                            dirtySegs.insert((idx - 1 + segCount) % segCount);
                            dirtySegs.insert(idx % segCount);
                            dirtySegs.insert((idx + 1) % segCount);
                        } else {
                            dirtySegs.insert(qBound(0, idx - 1, segCount - 1));
                            dirtySegs.insert(qBound(0, idx, segCount - 1));
                            dirtySegs.insert(qBound(0, idx + 1, segCount - 1));
                        }
                    }

                    if (bmHasTaper(properties)) {
                        int maxStartIdx = -1;
                        for (int si : startTaperSegments) maxStartIdx = qMax(maxStartIdx, si);

                        int minEndIdx = segCount;
                        for (int si : endTaperSegments) minEndIdx = qMin(minEndIdx, si);

                        bool needsStart = false;
                        bool needsEnd = false;
                        for (int si : dirtySegs) {
                            if (si <= maxStartIdx) needsStart = true;
                            if (si >= minEndIdx) needsEnd = true;
                        }
                        if (needsStart) dirtySegs.unite(startTaperSegments);
                        if (needsEnd) dirtySegs.unite(endTaperSegments);

                        const qreal lenDelta = qAbs(totalLen - mLastRasterTotalLength);
                        const qreal threshold = qMax<qreal>(2.0, properties.width * 0.25);
                        if (lenDelta > threshold) {
                            dirtySegs.unite(mLastRasterEndTaperSegments);
                            dirtySegs.unite(endTaperSegments);
                        }
                    }

                    QSet<quint64> dirtyTiles;
                    for (int si : dirtySegs) {
                        if (si >= 0 && si < mSegmentTiles.size()) {
                            for (quint64 k : mSegmentTiles[si]) dirtyTiles.insert(k);
                        }

                        if (si >= 0 && si < newSegPaths.size()) {
                            removeSegFromTiles(si);
                            const QRectF r = newSegPaths[si].boundingRect().adjusted(-pad, -pad, pad, pad);
                            const QVector<quint64> keys = tilesForRect(r);
                            for (quint64 k : keys) dirtyTiles.insert(k);
                            addSegToTiles(si, keys);

                            if (si >= mSegmentDabCenters.size()) {
                                mSegmentDabCenters.resize(si + 1);
                                mSegmentDabWidths.resize(si + 1);
                                mSegmentDabBounds.resize(si + 1);
                            }
                            bmBuildSegmentDabs(newSegPaths[si], newStartDists[si], totalLen, properties.width, startTaperLen, endTaperLen,
                                              mSegmentDabCenters[si], mSegmentDabWidths[si], mSegmentDabBounds[si]);
                        }
                    }

                    QSet<int> segsToRedraw;
                    for (quint64 k : dirtyTiles) {
                        auto it = mTileToSegments.find(k);
                        if (it == mTileToSegments.end()) continue;
                        segsToRedraw.unite(it.value());
                    }

                    const bool allowAsync = (dirtyTiles.size() > 48);
                    QSet<quint64> syncTiles;
                    QSet<quint64> asyncTiles;

                    if (allowAsync) {
                        // Keep the preview visually solid while dragging: redraw all *visible* dirty tiles synchronously.
                        // Bake off-screen tiles asynchronously.
                        const QRectF viewCanvasRect = (mEditor && mEditor->view())
                            ? mEditor->view()->mapScreenToCanvas(QRectF(0.0, 0.0, mScribbleArea->width(), mScribbleArea->height())).normalized()
                            : QRectF();
                        for (quint64 k : dirtyTiles) {
                            const QRectF tr = tileRect(k);
                            if (!viewCanvasRect.isEmpty() && tr.intersects(viewCanvasRect)) syncTiles.insert(k);
                            else asyncTiles.insert(k);
                        }
                    } else {
                        syncTiles = dirtyTiles;
                    }

                    // Invalidate baked images for dirty tiles and schedule background bake.
                    for (quint64 k : dirtyTiles) {
                        mBakedTileImages.remove(k);
                    }
                    if (allowAsync && !asyncTiles.isEmpty()) {
                        enqueueAsyncBakeTiles(asyncTiles);
                    } else if (!dirtyTiles.isEmpty()) {
                        enqueueAsyncBakeTiles(dirtyTiles);
                    }
                }

                mPrevSegmentPaths = newSegPaths;
                mPrevSegmentLengths = newSegLens;
                mSegmentStartDistances = newStartDists;
                mLastRasterTotalLength = totalLen;
                mLastRasterPointCount = n;
                mLastRasterClosed = isClosed;
                mLastRasterWidth = properties.width;
                mLastRasterColor = strokeColor.rgba();
                mLastRasterTaperStart = properties.curveTaperStart;
                mLastRasterTaperEnd = properties.curveTaperEnd;
                mLastRasterStartTaperSegments = startTaperSegments;
                mLastRasterEndTaperSegments = endTaperSegments;
            }
        }
    } else {
        if (mScribbleArea) {
            mScribbleArea->update();
        }
    }

    // Proactively update overlay region for crisp, artifact-free control points
    if (mScribbleArea) {
        QRect dirty = computeOverlayDirtyRect();
        if (!dirty.isEmpty()) {
            // Union with previous dirty to ensure old area is cleared
            QRect unionRect = dirty;
            if (!mLastOverlayRect.isEmpty()) unionRect = unionRect.united(mLastOverlayRect);
            // Slightly inflate to account for pen width and AA
            unionRect.adjust(-3, -3, 3, 3);
            mScribbleArea->update(unionRect);
            mLastOverlayRect = dirty;
        }
    }

    // Control points are now painted in paint() as a screen-space overlay to avoid blur
}

void BSplineTool::drawControlPoints()
{
    // Kept for compatibility; no-op since overlay painting has moved to paint()
}

void BSplineTool::paint(QPainter& painter, const QRect& blitRect)
{
    Q_UNUSED(blitRect)
    if (mPoints.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setWorldMatrixEnabled(false);

    QVector<QPointF> screenPts;
    screenPts.reserve(mPoints.size());
    for (const QPointF& p : mPoints)
        screenPts.push_back(mEditor->view()->mapCanvasToScreen(p));

    // Dashed lines connecting consecutive control points
    ToolOverlay::drawHandleChain(painter, screenPts, mClosed && screenPts.size() > 2);

    // Control point dots
    for (int i = 0; i < screenPts.size(); ++i) {
        const qreal r = (i == mHoverIndex) ? ToolOverlay::kDotRadius + 1.5 : ToolOverlay::kDotRadius;
        ToolOverlay::drawControlDotScreen(painter, screenPts[i], r);
    }

    painter.restore();
}

QPainterPath BSplineTool::buildCachedPath(const QList<QPointF>& pts)
{
    const int n = pts.size();
    
    if (n == 0) return QPainterPath();
    if (n == 1) {
        QPainterPath p;
        p.moveTo(pts[0]);
        return p;
    }
    
    // Ensure cache is sized properly
    if (mGeometryCache.segmentCount() != n) {
        mGeometryCache.resize(n);
        mGeometryCache.markAllDirty();
    }
    
    QPainterPath fullPath;
    
    if (n == 2) {
        // Simple line - check if cached
        if (mGeometryCache.isSegmentDirty(0)) {
            CurveSegmentCache& seg = mGeometryCache.getSegment(0);
            seg.segmentPath = QPainterPath();
            seg.segmentPath.moveTo(pts[0]);
            seg.segmentPath.lineTo(pts[1]);
            seg.length = QLineF(pts[0], pts[1]).length();
            seg.curvature = 0.0; // Straight line
            seg.optimalSamples = 2;
            seg.isDirty = false;
        }
        return mGeometryCache.getSegment(0).segmentPath;
    }
    
    // B-spline segments with adaptive caching
    if (mClosed) {
        QPointF startMid = midpoint(pts[0], pts[1]);
        fullPath.moveTo(startMid);
        
        for (int i = 1; i < n; ++i) {
            int iNext = (i + 1) % n;
            
            // Check if this segment needs recomputation
            if (mGeometryCache.isSegmentDirty(i)) {
                CurveSegmentCache& seg = mGeometryCache.getSegment(i);
                
                // Precompute segment path
                int iPrev = (i - 1 + n) % n;
                QPointF miPrev = midpoint(pts[iPrev], pts[i]);
                QPointF mi = midpoint(pts[i], pts[iNext]);
                
                seg.segmentPath = QPainterPath();
                seg.segmentPath.moveTo(miPrev);
                seg.segmentPath.quadTo(pts[i], mi);
                
                // Precompute length and curvature
                seg.length = seg.segmentPath.length();
                
                // Estimate curvature: measure deviation from straight line
                qreal straightDist = QLineF(miPrev, mi).length();
                seg.curvature = (straightDist > 1.0) ? qMin(1.0, seg.length / straightDist - 1.0) : 0.0;
                
                // Adaptive sampling: more samples for curved segments
                seg.optimalSamples = qBound(2, int(2 + seg.curvature * 10), 20);
                
                seg.isDirty = false;
            }
            
            // Reuse cached segment path
            const CurveSegmentCache& seg = mGeometryCache.getSegment(i);
            if (i == 1) {
                fullPath.connectPath(seg.segmentPath);
            } else {
                fullPath.connectPath(seg.segmentPath);
            }
        }
        
        // Final wrap segment
        if (mGeometryCache.isSegmentDirty(0)) {
            CurveSegmentCache& seg = mGeometryCache.getSegment(0);
            QPointF miPrev = midpoint(pts[n-1], pts[0]);
            QPointF mi = startMid;
            seg.segmentPath = QPainterPath();
            seg.segmentPath.moveTo(miPrev);
            seg.segmentPath.quadTo(pts[0], mi);
            seg.length = seg.segmentPath.length();
            qreal straightDist = QLineF(miPrev, mi).length();
            seg.curvature = (straightDist > 1.0) ? qMin(1.0, seg.length / straightDist - 1.0) : 0.0;
            seg.optimalSamples = qBound(2, int(2 + seg.curvature * 10), 20);
            seg.isDirty = false;
        }
        fullPath.connectPath(mGeometryCache.getSegment(0).segmentPath);
        fullPath.closeSubpath();
        
    } else {
        // Open B-spline
        QPointF m0 = midpoint(pts[0], pts[1]);
        fullPath.moveTo(pts[0]);
        fullPath.lineTo(m0);
        
        // Middle segments
        for (int i = 1; i <= n - 2; ++i) {
            if (mGeometryCache.isSegmentDirty(i)) {
                CurveSegmentCache& seg = mGeometryCache.getSegment(i);
                
                QPointF miPrev = midpoint(pts[i-1], pts[i]);
                QPointF mi = midpoint(pts[i], pts[i+1]);
                
                seg.segmentPath = QPainterPath();
                seg.segmentPath.moveTo(miPrev);
                seg.segmentPath.quadTo(pts[i], mi);
                
                seg.length = seg.segmentPath.length();
                qreal straightDist = QLineF(miPrev, mi).length();
                seg.curvature = (straightDist > 1.0) ? qMin(1.0, seg.length / straightDist - 1.0) : 0.0;
                seg.optimalSamples = qBound(2, int(2 + seg.curvature * 10), 20);
                seg.isDirty = false;
            }
            
            fullPath.connectPath(mGeometryCache.getSegment(i).segmentPath);
        }
        
        fullPath.lineTo(pts[n-1]);
    }
    
    return fullPath;
}

QRect BSplineTool::computeOverlayDirtyRect() const
{
    if (!mEditor || !mEditor->view()) return QRect();
    if (mPoints.isEmpty()) return QRect();

    // Compute screen-space bounding rect covering all control point circles
    const qreal baseRadiusPx = 7.5; // max of base and hover radii
    QRectF bounds;
    for (const QPointF& p : mPoints) {
        const QPointF s = mEditor->view()->mapCanvasToScreen(p);
        QRectF r(s.x() - baseRadiusPx, s.y() - baseRadiusPx, baseRadiusPx * 2, baseRadiusPx * 2);
        bounds = bounds.united(r);
    }

    return bounds.toAlignedRect();
}

static QPainterPath buildTaperedStroke(const QPainterPath& centerline, qreal fullWidth, qreal startTaperPixels, qreal endTaperPixels)
{
    const qreal totalLength = centerline.length();
    if (totalLength < 0.1 || fullWidth < 0.1) return QPainterPath();

    const qreal startTaper = qMax(0.0, startTaperPixels);
    const qreal endTaper = qMax(0.0, endTaperPixels);

    // Adaptive spacing with reasonable bounds
    const qreal baseSpacing = qMax(0.5, fullWidth * 0.08);
    const int samples = qBound(30, int(totalLength / baseSpacing) + 1, 800);

    QPainterPath result;

    for (int i = 0; i < samples; ++i) {
        const qreal t = qreal(i) / qreal(samples - 1);
        const qreal dist = t * totalLength;
        
        // Defensive: ensure percent is valid
        const qreal percent = qBound(0.0, centerline.percentAtLength(dist), 1.0);
        const QPointF center = centerline.pointAtPercent(percent);
        
        if (center.isNull()) continue; // Skip invalid points

        qreal startRamp = 1.0;
        if (startTaper > 0.0) startRamp = qMin(1.0, dist / startTaper);
        qreal endRamp = 1.0;
        if (endTaper > 0.0) endRamp = qMin(1.0, (totalLength - dist) / endTaper);
        const qreal ramp = qMin(startRamp, endRamp);
        const qreal width = qMax(0.5, fullWidth * ramp);
        const qreal radius = qBound(0.25, width * 0.5, fullWidth);

        result.addEllipse(center, radius, radius);
    }

    return result;
}

void BSplineTool::paintOverlay(QPainter& painter)
{
    if (mPreviewPath.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    ToolOverlay::drawPreviewPath(painter, mPreviewPath);
    painter.restore();
}


void BSplineTool::commitStroke()
{
    // Early exit: require at least 2 points for a valid stroke
    if (mPoints.size() < 2) return;

    Layer* layer = mEditor->layers()->currentLayer();

    // Early exit (vector-only): empty pre-rendered path means nothing to commit.
    // Bitmap commits use the already-rasterized preview buffer, not mPreRenderedStroke.
    if (layer && layer->type() == Layer::VECTOR) {
        if (mStrokePreRendered && mPreRenderedStroke.isEmpty()) return;
    }

    const UndoSaveState* saveState = mEditor->undoRedo()->state(UndoRedoRecordType::KEYFRAME_MODIFY);


    if (layer->type() == Layer::VECTOR) {
        // Convert quadratic segments to cubic and add to vector image
        BezierCurve curve;
        QList<QPointF> pts = mPoints;
        const int n = pts.size();
        if (!mClosed && n == 2) {
            // Straight line
            QPointF p0 = pts[0];
            QPointF p1 = pts[1];
            QPointF c1 = p0 + (p1 - p0) / 3.0;
            QPointF c2 = p0 + 2.0 * (p1 - p0) / 3.0;
            curve.setOrigin(p0);
            curve.appendCubic(c1, c2, p1, 1.0);
        } else if (n >= 3) {
            if (mClosed) {
                // Closed: start at midpoint(P0,P1) and wrap back to start using P0
                QPointF start = midpoint(pts[0], pts[1]);
                curve.setOrigin(start);
                for (int i = 1; i < n; ++i) {
                    int iPrev = (i - 1 + n) % n;
                    int iNext = (i + 1) % n;
                    QPointF miPrev = midpoint(pts[iPrev], pts[i]);
                    QPointF mi = midpoint(pts[i], pts[iNext]);
                    QPointF c = pts[i];
                    QPointF c1 = miPrev + (2.0/3.0) * (c - miPrev);
                    QPointF c2 = mi + (2.0/3.0) * (c - mi);
                    curve.appendCubic(c1, c2, mi, 1.0);
                }
                // Final wrap segment from mid(Pn-1,P0) to start using control P0
                QPointF miPrev = midpoint(pts[n-1], pts[0]);
                QPointF mi = start; // midpoint(pts[0], pts[1])
                QPointF c = pts[0];
                QPointF c1 = miPrev + (2.0/3.0) * (c - miPrev);
                QPointF c2 = mi + (2.0/3.0) * (c - mi);
                curve.appendCubic(c1, c2, mi, 1.0);
            } else {
                // Open stroke
                QPointF p0 = pts[0];
                QPointF m0 = midpoint(pts[0], pts[1]);
                QPointF c1cap = p0 + (m0 - p0) / 3.0;
                QPointF c2cap = p0 + 2.0 * (m0 - p0) / 3.0;
                curve.setOrigin(p0);
                curve.appendCubic(c1cap, c2cap, m0, 1.0);
                for (int i = 1; i <= n - 2; ++i) {
                    QPointF miPrev = midpoint(pts[i-1], pts[i]);
                    QPointF mi = midpoint(pts[i], pts[i+1]);
                    QPointF c = pts[i];
                    QPointF c1 = miPrev + (2.0/3.0) * (c - miPrev);
                    QPointF c2 = mi + (2.0/3.0) * (c - mi);
                    curve.appendCubic(c1, c2, mi, 1.0);
                }
                QPointF mLast = midpoint(pts[n-2], pts[n-1]);
                QPointF pn = pts[n-1];
                QPointF c1tail = mLast + (pn - mLast) / 3.0;
                QPointF c2tail = mLast + 2.0 * (pn - mLast) / 3.0;
                curve.appendCubic(c1tail, c2tail, pn, 1.0);
            }
        }

        if (mScribbleArea->makeInvisible()) {
            curve.setWidth(0);
        } else {
            curve.setWidth(properties.width);
        }
        curve.setColorNumber(mEditor->color()->frontColorNumber());
        curve.setVariableWidth(false);
        curve.setInvisibility(mScribbleArea->makeInvisible());

        VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame(), 0);
        if (vectorImage == nullptr) { return; }
        if (properties.drawBehind) {
            vectorImage->insertCurve(0, curve, mEditor->view()->scaling(), true);
        } else {
            vectorImage->addCurve(curve, mEditor->view()->scaling());
        }

        // Request raster re-render from newly added vector stroke
        if (mScribbleArea) {
            mScribbleArea->hybridStack()->rerenderRasterFromVectors(mEditor->currentFrame());
        }

    } else if (layer->type() == Layer::BITMAP) {
        const qreal effectiveWidth = [&]() -> qreal {
            qreal w = properties.width;
            if (mEditor && mEditor->tools() && mEditor->tools()->isGlobalWidthEnabled()) {
                const qreal gw = mEditor->tools()->globalWidth();
                if (gw > 0.0) { w = gw; }
            }
            return qMax<qreal>(1.0, w);
        }();

        // Use the already-built centerline — same fast path as polyline, instant commit.
        const QPainterPath& finalPath = mPreviewPath;
        if (finalPath.isEmpty()) { clearToolData(); return; }

        auto recordHybrid = [&]() {
            if (mEditor && mEditor->layers()) {
                auto* bitmapLayer = static_cast<LayerBitmap*>(layer);
                BitmapImage* cellImg = getSnapImage(bitmapLayer, mEditor->currentFrame());
                if (cellImg && !finalPath.isEmpty()) {
                    // Sample actual curve points so snap targets the visible stroke, not control points
                    cellImg->addHybridStroke(samplePathPoints(finalPath), effectiveWidth);
                }
            }
        };

        const bool finalLineActive = []() -> bool {
            QSettings s(PENCIL2D, PENCIL2D);
            s.beginGroup("PencilBrushFactory");
            const bool active = s.value("FinalLineActive", false).toBool();
            const QString presetId = s.value("ActivePresetId", QString()).toString();
            s.endGroup();
            return active || (presetId == QStringLiteral("__final_line__"));
        }();

        if (finalLineActive) {
            const QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                                       ? mEditor->tools()->strokeColorOverride()
                                       : mEditor->color()->frontColor();
            QPen pen(strokeColor);
            pen.setWidthF(effectiveWidth);
            pen.setCapStyle(Qt::SquareCap);
            pen.setJoinStyle(Qt::MiterJoin);
            if (mScribbleArea) {
                const bool useTapering = (properties.curveTaperStart > 0.0 || properties.curveTaperEnd > 0.0);
                if (useTapering)
                    mScribbleArea->drawTaperedCurve(finalPath, pen, true, properties.curveTaperStart, properties.curveTaperEnd, false, 0);
                else
                    mScribbleArea->drawPathCanvas(finalPath, pen, Qt::NoBrush, QPainter::CompositionMode_Source, false);
                mScribbleArea->commitCurrentSegmentToLayer();
                mScribbleArea->endStroke();
            }
            recordHybrid();
        } else {
            bool rendered = false;
            if (mEditor && mEditor->tools()) {
                if (auto* pencil = qobject_cast<PencilTool*>(mEditor->tools()->getTool(PENCIL))) {
                    if (pencil->renderStrokeAlongPathWithBaseWidth(finalPath, effectiveWidth, properties.curveTaperStart, properties.curveTaperEnd)) {
                        mScribbleArea->endStroke();
                        recordHybrid();
                        rendered = true;
                    }
                }
            }
            if (!rendered) {
                const QColor strokeColor = (mEditor && mEditor->tools() && mEditor->tools()->hasStrokeColorOverride())
                                           ? mEditor->tools()->strokeColorOverride()
                                           : mEditor->color()->frontColor();
                QPen pen(strokeColor, effectiveWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                const bool useTapering = (properties.curveTaperStart > 0.0 || properties.curveTaperEnd > 0.0);
                if (useTapering)
                    mScribbleArea->drawTaperedCurve(finalPath, pen, true, properties.curveTaperStart, properties.curveTaperEnd, false, 0);
                else
                    mScribbleArea->drawPolyline(finalPath, pen, false);
                mScribbleArea->endStroke();
                recordHybrid();
            }
        }
    }

    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    if (saveState) { mEditor->undoRedo()->record(saveState, typeName()); }
    clearToolData();
} 
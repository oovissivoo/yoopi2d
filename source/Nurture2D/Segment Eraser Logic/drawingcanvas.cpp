#include "drawingcanvas.h"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QLinearGradient>
#include <QLineF>
#include <QDebug>
#include <cmath>
#include <algorithm>

DrawingCanvas::DrawingCanvas(QWidget *parent)
    : QWidget(parent)
    , currentTool(BrushTool)
    , brushColor(Qt::black)
    , brushSize(5.0)
    , drawing(false)
    , drawBehindMode(false)
    , segmentEraserMode(false)
    , nextZIndex(1)
    , cacheValid(false)
    , frameTimeAverage(0.0)
    , frameCount(0)
{
    // Set default canvas size to 1280x720 (HD resolution)
    setFixedSize(1280, 720);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Initialize with a white background
    setStyleSheet("background-color: white;");

    // Initialize performance monitoring
    frameTimer.start();

    // Initialize pixmaps with proper size
    completedStrokesCache = QPixmap(size());
    completedStrokesCache.fill(Qt::transparent);

    currentStrokeBuffer = QPixmap(size());
    currentStrokeBuffer.fill(Qt::transparent);
    
    // Segment eraser initial state
    segmentEraseActive = false;
}

DrawingCanvas::~DrawingCanvas()
{
    // Qt handles cleanup automatically
}

void DrawingCanvas::setCurrentTool(Tool tool)
{
    currentTool = tool;
    if (drawing) {
        finishCurrentStroke();
    }
}

void DrawingCanvas::setBrushColor(const QColor &color)
{
    brushColor = color;
}

void DrawingCanvas::setBrushSize(qreal size)
{
    brushSize = qMax(0.5, qMin(200.0, size)); // Clamp size between 0.5 and 200
}

void DrawingCanvas::setDrawBehindMode(bool enabled)
{
    drawBehindMode = enabled;
}

void DrawingCanvas::setSegmentEraserMode(bool enabled)
{
    segmentEraserMode = enabled;
    // TODO: Implement segment eraser logic when needed
}

void DrawingCanvas::clearCanvas()
{
    strokes.clear();
    currentStroke = Stroke();

    // Clear all caches
    completedStrokesCache.fill(Qt::transparent);
    currentStrokeBuffer.fill(Qt::transparent);
    cacheValid = false;
    dirtyRegion = QRegion();

    update();
    emit canvasCleared();
}

void DrawingCanvas::paintEvent(QPaintEvent *event)
{
    frameTimer.restart();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Fill background
    painter.fillRect(rect(), Qt::white);

    // Only render what's needed based on the paint event region
    QRegion paintRegion = event->region();

    // Draw cached completed strokes if cache is valid
    if (cacheValid && !completedStrokesCache.isNull()) {
        painter.drawPixmap(0, 0, completedStrokesCache);
    } else if (!strokes.isEmpty()) {
        // Rebuild cache if needed
        updateCompletedStrokesCache();
        painter.drawPixmap(0, 0, completedStrokesCache);
    }

    // Draw current stroke if drawing
    if (drawing && !currentStroke.points.isEmpty()) {
        drawSmoothStroke(painter, currentStroke);
    }

    // Draw segment eraser detection line if active
    if (segmentEraseActive && segmentEraserLine.size() >= 2) {
        QPen detectionPen(QColor(0, 120, 255), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(detectionPen);
        for (int i = 1; i < segmentEraserLine.size(); ++i) {
            painter.drawLine(segmentEraserLine[i-1], segmentEraserLine[i]);
        }
    }

    // Draw border
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    // Update performance metrics
    qreal frameTime = frameTimer.elapsed();
    frameTimeAverage = (frameTimeAverage * frameCount + frameTime) / (frameCount + 1);
    frameCount++;
}

void DrawingCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (currentTool == EraserTool && segmentEraserMode) {
            // Start segment eraser line capture
            segmentEraseActive = true;
            segmentEraserLine.clear();
            segmentEraserLine.append(event->pos());
            segmentEraserRect = QRectF(event->pos(), QSizeF(1,1));
            updateDirtyRegion(segmentEraserRect.adjusted(-4, -4, 4, 4));
        } else {
            drawing = true;
            startNewStroke(event->pos());
        }
    }
}

void DrawingCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (segmentEraseActive && (event->buttons() & Qt::LeftButton)) {
        if (segmentEraserLine.isEmpty() || (segmentEraserLine.last() - event->pos()).manhattanLength() > MIN_DISTANCE) {
            segmentEraserLine.append(event->pos());
            segmentEraserRect = segmentEraserRect.united(QRectF(event->pos(), QSizeF(1,1)));
            updateDirtyRegion(segmentEraserRect.adjusted(-6, -6, 6, 6));
        }
    } else if (drawing && (event->buttons() & Qt::LeftButton)) {
        addPointToCurrentStroke(event->pos());

        // Update only the dirty region for better performance
        if (!currentStroke.points.isEmpty()) {
            QRectF updateRect = currentStroke.boundingRect.adjusted(-2, -2, 2, 2);
            updateDirtyRegion(updateRect);
        }
    }
}

void DrawingCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (segmentEraseActive) {
            // Detection line only selects strokes to operate on.
            if (segmentEraserLine.size() >= 2) {
                auto lerp = [](const QPointF &a, const QPointF &b, double t) -> QPointF {
                    return QPointF(a.x() + (b.x()-a.x())*t, a.y() + (b.y()-a.y())*t);
                };

                auto segIntersectDetailed = [](const QPointF &a1, const QPointF &a2, const QPointF &b1, const QPointF &b2, double &tA, double &tB) -> bool {
                    double x1=a1.x(), y1=a1.y(), x2=a2.x(), y2=a2.y();
                    double x3=b1.x(), y3=b1.y(), x4=b2.x(), y4=b2.y();
                    double den=(x1-x2)*(y3-y4)-(y1-y2)*(x3-x4);
                    if (std::abs(den) < 1e-8) return false;
                    double pre=x1*y2 - y1*x2;
                    double post=x3*y4 - y3*x4;
                    double xi=(pre*(x3-x4)-(x1-x2)*post)/den;
                    double yi=(pre*(y3-y4)-(y1-y2)*post)/den;
                    // parameters along each segment
                    if (std::abs(x2-x1) > std::abs(y2-y1)) tA = (xi - x1)/((x2 - x1) == 0 ? 1 : (x2 - x1));
                    else tA = (yi - y1)/((y2 - y1) == 0 ? 1 : (y2 - y1));
                    if (std::abs(x4-x3) > std::abs(y4-y3)) tB = (xi - x3)/((x4 - x3) == 0 ? 1 : (x4 - x3));
                    else tB = (yi - y3)/((y4 - y3) == 0 ? 1 : (y4 - y3));
                    if (tA < -1e-6 || tA > 1+1e-6 || tB < -1e-6 || tB > 1+1e-6) return false;
                    return true;
                };

                auto extractSubPolyline = [&](const QVector<QPointF> &pts, double fromPos, double toPos) -> QVector<QPointF> {
                    QVector<QPointF> out;
                    if (pts.isEmpty()) return out;
                    if (fromPos > toPos) std::swap(fromPos, toPos);
                    fromPos = qBound(0.0, fromPos, (double)pts.size()-1.0);
                    toPos = qBound(0.0, toPos, (double)pts.size()-1.0);
                    int sIdx = (int)std::floor(fromPos);
                    double sT = fromPos - sIdx;
                    int eIdx = (int)std::floor(toPos);
                    double eT = toPos - eIdx;

                    QPointF start = sT <= 1e-8 ? pts[sIdx] : lerp(pts[sIdx], pts[sIdx+1], sT);
                    QPointF end = eT <= 1e-8 ? pts[eIdx] : lerp(pts[eIdx], pts[eIdx+1], eT);

                    out.append(start);
                    for (int i = sIdx+1; i <= eIdx; ++i) out.append(pts[i]);
                    out.last() = end;
                    return out;
                };

                // Detection line bounding rect for quick rejection
                QRectF detectRect;
                for (const QPointF &p : segmentEraserLine) {
                    detectRect = detectRect.isValid() ? detectRect.united(QRectF(p, QSizeF(1,1))) : QRectF(p, QSizeF(1,1));
                }

                QVector<Stroke> result;
                result.reserve(strokes.size());

                // Helper to collect intersection params of a stroke with ALL strokes (and itself)
                auto collectStrokeBoundaries = [&](const Stroke &s) {
                    QVector<double> bounds;
                    // with other strokes
                    for (const Stroke &o : strokes) {
                        for (int i = 1; i < s.points.size(); ++i) {
                            QPointF a1 = s.points[i-1], a2 = s.points[i];
                            for (int j = 1; j < o.points.size(); ++j) {
                                // Skip adjacent self neighbors
                                if (&o == &s && (std::abs(i - j) <= 1)) continue;
                                QPointF b1 = o.points[j-1], b2 = o.points[j];
                                double tA, tB;
                                if (segIntersectDetailed(a1,a2,b1,b2,tA,tB)) {
                                    double pos = (i-1) + qBound(0.0, tA, 1.0);
                                    bounds.append(pos);
                                }
                            }
                        }
                    }
                    std::sort(bounds.begin(), bounds.end());
                    // unique with tolerance
                    QVector<double> uniq; uniq.reserve(bounds.size());
                    double last = std::numeric_limits<double>::infinity();
                    for (double v : bounds) {
                        if (!std::isfinite(last) || std::abs(v - last) > 1e-4) { uniq.append(v); last = v; }
                    }
                    return uniq;
                };

                for (const Stroke &s : strokes) {
                    if (s.points.size() < 2) { result.append(s); continue; }
                    QRectF sr = s.boundingRect.adjusted(-s.size, -s.size, s.size, s.size);
                    if (!sr.intersects(detectRect)) { result.append(s); continue; }

                    // 1) Find detection crossings to know which strokes to operate on
                    QVector<double> detectionPos;
                    for (int i = 1; i < s.points.size(); ++i) {
                        QPointF a1 = s.points[i-1], a2 = s.points[i];
                        for (int j = 1; j < segmentEraserLine.size(); ++j) {
                            QPointF b1 = segmentEraserLine[j-1], b2 = segmentEraserLine[j];
                            double tA, tB;
                            if (segIntersectDetailed(a1,a2,b1,b2,tA,tB)) {
                                detectionPos.append((i-1) + qBound(0.0, tA, 1.0));
                            }
                        }
                    }
                    if (detectionPos.isEmpty()) { result.append(s); continue; }

                    // 2) Compute boundaries = intersections with any stroke (including itself)
                    QVector<double> boundaries = collectStrokeBoundaries(s);

                    // 3) Build removal intervals bracketing each detection position
                    struct Interval { double a; double b; };
                    QVector<Interval> toRemove;
                    const double startPos = 0.0, endPos = (double)s.points.size()-1.0;
                    for (double pos : detectionPos) {
                        double left = startPos;
                        double right = endPos;
                        for (double b : boundaries) {
                            if (b < pos) left = b; else { right = b; break; }
                        }
                        // Remove from left to right
                        if (right - left > 1e-6) toRemove.append({left, right});
                    }

                    if (toRemove.isEmpty()) {
                        // If no boundaries anywhere, remove whole stroke per spec
                        if (boundaries.isEmpty()) {
                            // skip adding this stroke (fully erased)
                            continue;
                        } else {
                            result.append(s);
                            continue;
                        }
                    }

                    std::sort(toRemove.begin(), toRemove.end(), [](const Interval &x, const Interval &y){ return x.a < y.a; });
                    // Merge overlapping intervals
                    QVector<Interval> merged;
                    for (const auto &iv : toRemove) {
                        if (merged.isEmpty() || iv.a > merged.last().b) merged.append(iv);
                        else merged.last().b = std::max(merged.last().b, iv.b);
                    }

                    // Complement -> kept segments
                    double cur = startPos;
                    for (const auto &rm : merged) {
                        if (cur < rm.a) {
                            QVector<QPointF> seg = extractSubPolyline(s.points, cur, rm.a);
                            if (seg.size() >= 2) { Stroke keep = s; keep.points = seg; keep.updateBoundingRect(); result.append(keep); }
                        }
                        cur = rm.b;
                    }
                    if (cur < endPos) {
                        QVector<QPointF> seg = extractSubPolyline(s.points, cur, endPos);
                        if (seg.size() >= 2) { Stroke keep = s; keep.points = seg; keep.updateBoundingRect(); result.append(keep); }
                    }
                }

                strokes = result;
                cacheValid = false;
                updateCompletedStrokesCache();
            }

            // reset line
            segmentEraseActive = false;
            segmentEraserLine.clear();
            segmentEraserRect = QRectF();
            update();
        } else if (drawing) {
            finishCurrentStroke();
            drawing = false;
        }
    }
}

void DrawingCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // Recreate pixmaps with new size
    completedStrokesCache = QPixmap(size());
    completedStrokesCache.fill(Qt::transparent);

    currentStrokeBuffer = QPixmap(size());
    currentStrokeBuffer.fill(Qt::transparent);

    cacheValid = false;
    updateCompletedStrokesCache();
}

void DrawingCanvas::drawStroke(QPainter &painter, const Stroke &stroke)
{
    if (stroke.points.size() < 2) {
        return; // Need at least 2 points to draw a line
    }

    // Set up painter for this stroke
    if (stroke.isEraser) {
        painter.setPen(QPen(Qt::white, stroke.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setCompositionMode(QPainter::CompositionMode_Source);
    } else {
        painter.setPen(QPen(stroke.color, stroke.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    // Draw smooth curve through the points
    QPainterPath path;
    path.moveTo(stroke.points.first());

    if (stroke.points.size() == 2) {
        // Simple line for just 2 points
        path.lineTo(stroke.points.last());
    } else {
        // Use quadratic curves for smoother lines
        for (int i = 1; i < stroke.points.size() - 1; ++i) {
            QPointF cp = (stroke.points[i-1] + stroke.points[i+1]) / 2.0;
            path.quadTo(stroke.points[i], cp);
        }

        // Connect to the last point
        if (stroke.points.size() > 2) {
            path.lineTo(stroke.points.last());
        }
    }

    painter.drawPath(path);
}

void DrawingCanvas::drawSmoothStroke(QPainter &painter, const Stroke &stroke)
{
    if (stroke.points.size() < 2) {
        return;
    }

    // High-quality stroke rendering with optimizations
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

    // Set up painter with optimal settings
    if (stroke.isEraser) {
        painter.setPen(QPen(Qt::white, stroke.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setCompositionMode(QPainter::CompositionMode_Source);
    } else {
        // Add slight transparency for smoother blending
        QColor strokeColor = stroke.color;
        strokeColor.setAlpha(240);
        painter.setPen(QPen(strokeColor, stroke.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    // Create optimized path with better curve fitting
    QPainterPath path;
    path.moveTo(stroke.points.first());

    if (stroke.points.size() == 2) {
        path.lineTo(stroke.points.last());
    } else {
        // Advanced smoothing algorithm using Catmull-Rom spline approximation
        for (int i = 1; i < stroke.points.size() - 2; ++i) {
            const QPointF &p0 = stroke.points[i-1];
            const QPointF &p1 = stroke.points[i];
            const QPointF &p2 = stroke.points[i+1];
            const QPointF &p3 = stroke.points[i+2];

            // Calculate control points for smooth curve
            QPointF cp1 = p1 + (p2 - p0) * 0.1667; // 1/6
            QPointF cp2 = p2 - (p3 - p1) * 0.1667; // 1/6

            path.cubicTo(cp1, cp2, p2);
        }

        // Handle remaining points
        if (stroke.points.size() > 2) {
            path.lineTo(stroke.points.last());
        }
    }

    painter.drawPath(path);
}

void DrawingCanvas::startNewStroke(const QPointF &point)
{
    currentStroke = Stroke(
        currentTool == EraserTool ? Qt::white : brushColor,
        brushSize,
        currentTool == EraserTool
    );

    // Set z-index based on draw behind mode and current tool
    if (currentTool == BrushTool && drawBehindMode) {
        // Find the lowest z-index among existing strokes and place behind it
        int lowestZIndex = nextZIndex;
        for (const Stroke &stroke : strokes) {
            if (!stroke.isEraser) { // Only consider brush strokes for layering
                lowestZIndex = qMin(lowestZIndex, stroke.zIndex);
            }
        }
        currentStroke.zIndex = lowestZIndex - 1;
    } else {
        currentStroke.zIndex = nextZIndex++;
    }

    currentStroke.points.append(point);
    currentStroke.updateBoundingRect();
}

void DrawingCanvas::addPointToCurrentStroke(const QPointF &point)
{
    // Only add point if it's significantly different from the last point
    if (currentStroke.points.isEmpty() ||
        (currentStroke.points.last() - point).manhattanLength() > MIN_DISTANCE) {

        currentStroke.points.append(point);
        currentStroke.updateBoundingRect();

        // Optimize stroke if it gets too large
        if (currentStroke.points.size() > 100) {
            optimizeStroke(currentStroke);
        }
    }
}

void DrawingCanvas::finishCurrentStroke()
{
    if (!currentStroke.points.isEmpty()) {
        // Optimize the stroke before adding it
        optimizeStroke(currentStroke);

        strokes.append(currentStroke);
        currentStroke = Stroke();

        // Update the cache with the new completed stroke
        updateCompletedStrokesCache();

        emit strokeAdded();
    }
}

void DrawingCanvas::optimizeStroke(Stroke &stroke)
{
    if (stroke.points.size() < 3) return;

    // Douglas-Peucker algorithm for stroke simplification
    QVector<QPointF> optimizedPoints;
    optimizedPoints.reserve(stroke.points.size());

    // Add first point
    optimizedPoints.append(stroke.points.first());

    // Simplify intermediate points
    for (int i = 1; i < stroke.points.size() - 1; ++i) {
        const QPointF &current = stroke.points[i];
        const QPointF &prev = optimizedPoints.last();

        // Only keep points that deviate significantly from the line
        if ((current - prev).manhattanLength() > MIN_DISTANCE) {
            optimizedPoints.append(current);
        }
    }

    // Add last point
    if (!stroke.points.isEmpty()) {
        optimizedPoints.append(stroke.points.last());
    }

    stroke.points = optimizedPoints;
    stroke.updateBoundingRect();
}

void DrawingCanvas::updateCompletedStrokesCache()
{
    if (strokes.isEmpty()) {
        completedStrokesCache.fill(Qt::transparent);
        cacheValid = true;
        return;
    }

    // Clear cache and redraw all completed strokes
    completedStrokesCache.fill(Qt::transparent);

    QPainter cachePainter(&completedStrokesCache);
    cachePainter.setRenderHint(QPainter::Antialiasing);
    cachePainter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Sort strokes by z-index to ensure proper layering
    QVector<Stroke> sortedStrokes = strokes;
    std::sort(sortedStrokes.begin(), sortedStrokes.end(),
              [](const Stroke &a, const Stroke &b) {
                  return a.zIndex < b.zIndex; // Lower z-index = drawn first (behind)
              });

    // Draw all completed strokes to cache in correct order
    for (const Stroke &stroke : sortedStrokes) {
        if (stroke.points.size() >= 2) {
            drawSmoothStroke(cachePainter, stroke);
        }
    }

    cacheValid = true;
}

void DrawingCanvas::updateDirtyRegion(const QRectF &region)
{
    dirtyRegion += region.toRect();
    update(dirtyRegion);
    dirtyRegion = QRegion(); // Clear after update
}

void DrawingCanvas::renderToPixmap(QPixmap &pixmap, const QVector<Stroke> &strokesToRender, const QRect &clipRect)
{
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!clipRect.isNull()) {
        painter.setClipRect(clipRect);
    }

    for (const Stroke &stroke : strokesToRender) {
        if (stroke.points.size() >= 2) {
            drawSmoothStroke(painter, stroke);
        }
    }
}

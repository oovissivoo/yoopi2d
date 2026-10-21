#ifndef DRAWINGCANVAS_H
#define DRAWINGCANVAS_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QPen>
#include <QPainterPath>
#include <QRegion>
#include <QElapsedTimer>

struct Stroke
{
    QVector<QPointF> points;
    QColor color;
    qreal size;
    bool isEraser;
    QRectF boundingRect;
    int zIndex; // Z-index for layering (lower = behind, higher = front)

    Stroke() : color(Qt::black), size(5.0), isEraser(false), zIndex(0) {}
    Stroke(const QColor &c, qreal s, bool eraser = false)
        : color(c), size(s), isEraser(eraser), zIndex(0) {}

    void updateBoundingRect() {
        if (points.isEmpty()) return;
        qreal halfSize = size / 2.0 + 1.0; // Extra padding for anti-aliasing
        qreal minX = points[0].x(), maxX = points[0].x();
        qreal minY = points[0].y(), maxY = points[0].y();

        for (const QPointF &p : points) {
            minX = qMin(minX, p.x());
            maxX = qMax(maxX, p.x());
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }

        boundingRect = QRectF(minX - halfSize, minY - halfSize,
                             maxX - minX + size + 2.0, maxY - minY + size + 2.0);
    }
};

class DrawingCanvas : public QWidget
{
    Q_OBJECT

public:
    enum Tool { BrushTool, EraserTool };

    explicit DrawingCanvas(QWidget *parent = nullptr);
    ~DrawingCanvas() override;

    void setCurrentTool(Tool tool);
    void setBrushColor(const QColor &color);
    void setBrushSize(qreal size);
    void setDrawBehindMode(bool enabled);
    void setSegmentEraserMode(bool enabled);

    void clearCanvas();

    // Performance monitoring
    int getStrokeCount() const { return strokes.size(); }
    qreal getAverageFrameTime() const { return frameTimeAverage; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // Core drawing functions
    void drawStroke(QPainter &painter, const Stroke &stroke);
    void drawSmoothStroke(QPainter &painter, const Stroke &stroke);
    void startNewStroke(const QPointF &point);
    void addPointToCurrentStroke(const QPointF &point);
    void finishCurrentStroke();
    void optimizeStroke(Stroke &stroke);
    void startSegmentErase(const QPointF &point);
    void addPointToSegmentErase(const QPointF &point);
    void finishSegmentErase();

    // Performance optimization functions
    void updateCompletedStrokesCache();
    void updateDirtyRegion(const QRectF &region);
    void renderToPixmap(QPixmap &pixmap, const QVector<Stroke> &strokesToRender, const QRect &clipRect = QRect());

    // Canvas properties
    Tool currentTool;
    QColor brushColor;
    qreal brushSize;
    bool drawing;
    bool drawBehindMode;
    bool segmentEraserMode;
    int nextZIndex;

    // Drawing data
    QVector<Stroke> strokes;
    Stroke currentStroke;

    // Segment eraser state
    bool segmentEraseActive;
    QVector<QPointF> segmentEraserLine; // Blue detection line points
    QRectF segmentEraserRect;           // Bounding rect for incremental updates

    // Performance optimization layers
    QPixmap completedStrokesCache;  // Cached completed strokes
    QPixmap currentStrokeBuffer;    // Current stroke being drawn
    QRegion dirtyRegion;            // Region that needs updating
    bool cacheValid;                // Whether the cache is up to date

    // Performance monitoring
    QElapsedTimer frameTimer;
    qreal frameTimeAverage;
    int frameCount;

    // Smoothing parameters
    static constexpr int MIN_DISTANCE = 2;  // Minimum pixel distance between points
    static constexpr int SMOOTHING_ITERATIONS = 2;

signals:
    void strokeAdded();
    void canvasCleared();
};

#endif // DRAWINGCANVAS_H

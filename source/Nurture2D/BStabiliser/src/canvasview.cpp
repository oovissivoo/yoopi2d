#include "canvasview.h"

#include <QCursor>
#include <QGraphicsPixmapItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

namespace {
constexpr int kCanvasWidth = 512;
constexpr int kCanvasHeight = 300;
constexpr qreal kMinScale = 0.25;
constexpr qreal kMaxScale = 5.0;
constexpr int kMaxUndoStates = 50;
}

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_baseItem(nullptr)
    , m_previewItem(nullptr)
    , m_baseImage(kCanvasWidth, kCanvasHeight, QImage::Format_ARGB32_Premultiplied)
    , m_previewImage(kCanvasWidth, kCanvasHeight, QImage::Format_ARGB32_Premultiplied)
    , m_pen(Qt::black, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
    , m_isDrawing(false)
    , m_spacePressed(false)
    , m_isPanning(false)
    , m_lastAddedPoint()
    , m_currentPressure(1.0)
    , m_tabletInUse(false)
    , m_currentScale(1.0)
{
    setScene(m_scene);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setBackgroundBrush(QColor(0x2b, 0x2b, 0x2b));
    setDragMode(QGraphicsView::NoDrag);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setAlignment(Qt::AlignCenter);

    initializeScene();
    updateUndoRedoState();
}

void CanvasView::initializeScene()
{
    // Base canvas
    m_baseImage.fill(Qt::white);
    m_basePixmap = QPixmap::fromImage(m_baseImage);
    if (!m_baseItem) {
        m_baseItem = m_scene->addPixmap(m_basePixmap);
        m_baseItem->setTransformationMode(Qt::SmoothTransformation);
    } else {
        m_baseItem->setPixmap(m_basePixmap);
    }

    // Raster preview layer (transparent initially)
    m_previewImage.fill(Qt::transparent);
    if (!m_previewItem) {
        m_previewItem = m_scene->addPixmap(QPixmap::fromImage(m_previewImage));
        m_previewItem->setZValue(2.0);
        m_previewItem->setTransformationMode(Qt::SmoothTransformation);
    } else {
        m_previewItem->setPixmap(QPixmap::fromImage(m_previewImage));
    }
    m_previewItem->setVisible(false);

    // Large scene rect for panning
    const qreal margin = 2000.0;
    QRectF sceneRect = m_baseItem->boundingRect().marginsAdded(QMarginsF(margin, margin, margin, margin));
    m_scene->setSceneRect(sceneRect);
    centerOn(m_baseItem);
}

void CanvasView::setBrushSize(int size)
{
    m_pen.setWidth(size);
}

void CanvasView::setPenColor(const QColor &color)
{
    // Final stroke always blue; UI pen can be set for other tools.
    m_pen.setColor(color);
}

void CanvasView::setSmoothFactor(float factor)
{
    m_smoothFactor = qBound(0.0f, factor, 1.0f);
}

void CanvasView::setSmoothIterations(int iterations)
{
    m_smoothIterations = qBound(0, iterations, 10);
}

void CanvasView::clearCanvas()
{
    pushUndoState();
    m_baseImage.fill(Qt::white);
    refreshBasePixmap();
    m_vectorPoints.clear();
    if (m_previewItem) {
        m_previewItem->setVisible(false);
    }
    m_redoStack.clear();
    updateUndoRedoState();
}

bool CanvasView::canUndo() const { return !m_undoStack.isEmpty(); }
bool CanvasView::canRedo() const { return !m_redoStack.isEmpty(); }

void CanvasView::undo()
{
    if (!canUndo()) return;
    m_redoStack.push_back(m_baseImage);
    m_baseImage = m_undoStack.takeLast();
    refreshBasePixmap();
    m_vectorPoints.clear();
    if (m_previewItem) {
        m_previewItem->setVisible(false);
    }
    updateUndoRedoState();
}

void CanvasView::redo()
{
    if (!canRedo()) return;
    m_undoStack.push_back(m_baseImage);
    m_baseImage = m_redoStack.takeLast();
    refreshBasePixmap();
    m_vectorPoints.clear();
    if (m_previewItem) {
        m_previewItem->setVisible(false);
    }
    updateUndoRedoState();
}

void CanvasView::tabletEvent(QTabletEvent *event)
{
    m_tabletInUse = true;
    
    switch (event->type()) {
        case QEvent::TabletPress:
            if (!m_spacePressed) {
                pushUndoState();
                m_isDrawing = true;
                m_lastCanvasPos = mapToCanvas(event->pos());
                m_currentPressure = event->pressure();
                beginStroke();
            }
            event->accept();
            break;
            
        case QEvent::TabletMove:
            if (m_isDrawing) {
                m_lastCanvasPos = mapToCanvas(event->pos());
                m_currentPressure = event->pressure();
                updateVectorPreview(StrokePoint(m_lastCanvasPos, m_currentPressure));
            }
            event->accept();
            break;
            
        case QEvent::TabletRelease:
            if (m_isDrawing) {
                m_isDrawing = false;
                commitVectorStroke();
            }
            event->accept();
            break;
            
        default:
            break;
    }
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (m_tabletInUse) {
        event->accept();
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        if (m_spacePressed) {
            m_isPanning = true;
            m_lastMousePos = event->pos();
            setCursor(Qt::ClosedHandCursor);
        } else {
            pushUndoState();
            m_isDrawing = true;
            m_lastCanvasPos = mapToCanvas(event->pos());
            m_currentPressure = 1.0; // Full pressure for mouse
            beginStroke();
            m_redoStack.clear();
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_tabletInUse) {
        event->accept();
        return;
    }
    
    if (m_isPanning) {
        QPoint cur = event->pos();
        QPoint delta = cur - m_lastMousePos;
        m_lastMousePos = cur;
        QPointF center = mapToScene(viewport()->rect().center()) - QPointF(delta);
        centerOn(center);
        event->accept();
        return;
    }
    if (m_isDrawing) {
        m_lastCanvasPos = mapToCanvas(event->pos());
        m_currentPressure = 1.0; // Full pressure for mouse
        updateVectorPreview(StrokePoint(m_lastCanvasPos, m_currentPressure));
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_tabletInUse) {
        m_tabletInUse = false;
        event->accept();
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        if (m_isPanning) {
            m_isPanning = false;
            setCursor(m_spacePressed ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
        if (m_isDrawing) {
            m_isDrawing = false;
            commitVectorStroke();
        }
        updateUndoRedoState();
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    QPoint numPixels = event->pixelDelta();
    QPoint numDegrees = event->angleDelta();
    if (!numPixels.isNull())
        numDegrees = numPixels * 8;
    if (numDegrees.y() == 0) { event->ignore(); return; }
    qreal factor = (numDegrees.y() > 0) ? 1.15 : 1.0/1.15;
    qreal newScale = m_currentScale * factor;
    newScale = qBound(kMinScale, newScale, kMaxScale);
    qreal applied = newScale / m_currentScale;
    if (!qFuzzyCompare(applied, 1.0)) {
        scale(applied, applied);
        m_currentScale = newScale;
    }
    event->accept();
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = true;
        if (!m_isPanning) setCursor(Qt::OpenHandCursor);
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = false;
        if (!m_isPanning) unsetCursor();
        else setCursor(Qt::ClosedHandCursor);
    }
    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::enterEvent(QEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    QGraphicsView::enterEvent(event);
}

void CanvasView::leaveEvent(QEvent *event)
{
    if (!m_isPanning) unsetCursor();
    QGraphicsView::leaveEvent(event);
}

void CanvasView::refreshBasePixmap(const QRect &dirtyRegion)
{
    if (!m_baseItem) return;
    if (dirtyRegion.isValid()) {
        QImage sub = m_baseImage.copy(dirtyRegion);
        QPainter p(&m_basePixmap);
        p.drawImage(dirtyRegion.topLeft(), sub);
    } else {
        m_basePixmap = QPixmap::fromImage(m_baseImage);
    }
    m_baseItem->setPixmap(m_basePixmap);
}

QPointF CanvasView::mapToCanvas(const QPoint &viewPoint) const
{
    if (!m_baseItem) return QPointF();
    QPointF scenePt = mapToScene(viewPoint);
    QPointF itemPt = m_baseItem->mapFromScene(scenePt);
    itemPt.setX(qBound<qreal>(0, itemPt.x(), m_baseImage.width() - 1));
    itemPt.setY(qBound<qreal>(0, itemPt.y(), m_baseImage.height() - 1));
    return itemPt;
}

void CanvasView::pushUndoState()
{
    if (m_undoStack.size() >= kMaxUndoStates) m_undoStack.remove(0);
    m_undoStack.push_back(m_baseImage);
    updateUndoRedoState();
}

void CanvasView::updateUndoRedoState()
{
    emit undoAvailabilityChanged(canUndo());
    emit redoAvailabilityChanged(canRedo());
}

void CanvasView::beginStroke()
{
    m_vectorPoints.clear();
    m_rawStrokePoints.clear();
    // Add first point with pressure
    m_rawStrokePoints.append(StrokePoint(m_lastCanvasPos, m_currentPressure));
    m_lastAddedPoint = m_lastCanvasPos; // Track for distance filtering
    // Clear and show preview layer
    m_previewImage.fill(Qt::transparent);
    if (m_previewItem) {
        m_previewItem->setPixmap(QPixmap::fromImage(m_previewImage));
        m_previewItem->setVisible(true);
    }
}

void CanvasView::updateVectorPreview(const StrokePoint &pt)
{
    // Distance-based filtering to prevent tablet oversampling
    // Use larger threshold for tablets to make smoothing more effective
    qreal minDistance = m_tabletInUse ? 6.0 : 2.0;
    qreal dist = QLineF(m_lastAddedPoint, pt.pos).length();
    if (dist < minDistance) {
        return; // Skip this point - too close to last one
    }
    
    // Add raw point with pressure
    m_rawStrokePoints.append(pt);
    m_lastAddedPoint = pt.pos; // Update last added point
    
    // Apply smoothing to get control points
    // Use enhanced smoothing for tablet input
    float effectiveSmoothFactor = m_smoothFactor;
    int effectiveIterations = m_smoothIterations;
    if (m_tabletInUse) {
        effectiveSmoothFactor = qMin(1.0f, m_smoothFactor * m_tabletSmoothMultiplier);
        effectiveIterations = qMin(10, m_smoothIterations + m_tabletIterationsBoost);
    }
    m_vectorPoints = smoothPoints(m_rawStrokePoints, effectiveSmoothFactor, effectiveIterations);
    
    if (m_vectorPoints.size() < 2) {
        return;
    }
    
    // Generate densely sampled curve points for smooth rendering
    QVector<StrokePoint> curvePoints = generateCurvePoints(m_vectorPoints);
    
    // Clear and redraw preview image with raster lines
    m_previewImage.fill(Qt::transparent);
    QPainter painter(&m_previewImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    drawRasterStroke(painter, curvePoints, m_pen.width());
    
    painter.end();
    
    // Update preview pixmap
    if (m_previewItem) {
        m_previewItem->setPixmap(QPixmap::fromImage(m_previewImage));
    }
}

void CanvasView::commitVectorStroke()
{
    if (m_rawStrokePoints.size() < 2) {
        if (m_previewItem) m_previewItem->setVisible(false);
        return;
    }
    // Apply final smoothing to get control points
    // Use enhanced smoothing for tablet input
    float effectiveSmoothFactor = m_smoothFactor;
    int effectiveIterations = m_smoothIterations;
    if (m_tabletInUse) {
        effectiveSmoothFactor = qMin(1.0f, m_smoothFactor * m_tabletSmoothMultiplier);
        effectiveIterations = qMin(10, m_smoothIterations + m_tabletIterationsBoost);
    }
    QVector<StrokePoint> controlPoints = smoothPoints(m_rawStrokePoints, effectiveSmoothFactor, effectiveIterations);
    
    // Generate densely sampled curve points for smooth rendering
    QVector<StrokePoint> curvePoints = generateCurvePoints(controlPoints);
    
    // Draw pure raster stroke
    QPainter painter(&m_baseImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    drawRasterStroke(painter, curvePoints, m_pen.width());
    
    painter.end();
    
    // Calculate dirty region
    QRectF bounds;
    for (const StrokePoint &pt : curvePoints) {
        qreal radius = m_pen.width() * pt.pressure;
        bounds = bounds.united(QRectF(pt.pos.x() - radius, pt.pos.y() - radius, 
                                       radius * 2, radius * 2));
    }
    QRect dirty = bounds.toAlignedRect().adjusted(-1, -1, 1, 1);
    refreshBasePixmap(dirty);
    
    // Clear preview
    m_vectorPoints.clear();
    m_rawStrokePoints.clear();
    if (m_previewItem) {
        m_previewItem->setVisible(false);
    }
}

void CanvasView::drawRasterStroke(QPainter &painter, const QVector<StrokePoint> &points, int basePenWidth)
{
    if (points.size() < 2) {
        return;
    }
    
    QPen pen(Qt::blue);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    
    // Draw lines with pressure-varying width
    for (int i = 0; i < points.size() - 1; ++i) {
        const StrokePoint &p1 = points[i];
        const StrokePoint &p2 = points[i + 1];
        
        // Average pressure between consecutive points
        qreal avgPressure = (p1.pressure + p2.pressure) / 2.0;
        int width = qMax(1, (int)(basePenWidth * avgPressure));
        
        pen.setWidth(width);
        painter.setPen(pen);
        painter.drawLine(p1.pos, p2.pos);
    }
}

QVector<StrokePoint> CanvasView::smoothPoints(const QVector<StrokePoint> &points, float factor, int iterations) const
{
    if (points.size() < 3 || factor <= 0.0f || iterations <= 0) {
        return points;
    }
    
    QVector<StrokePoint> smoothed = points;
    
    // Apply smoothing iterations
    for (int iter = 0; iter < iterations; ++iter) {
        QVector<StrokePoint> temp = smoothed;
        
        // Smooth each interior point (preserve first and last)
        for (int i = 1; i < smoothed.size() - 1; ++i) {
            // Average with neighbors (Blender-style)
            QPointF prevPos = smoothed[i - 1].pos;
            QPointF currPos = smoothed[i].pos;
            QPointF nextPos = smoothed[i + 1].pos;
            
            qreal prevPressure = smoothed[i - 1].pressure;
            qreal currPressure = smoothed[i].pressure;
            qreal nextPressure = smoothed[i + 1].pressure;
            
            // Calculate average position and pressure
            QPointF avgPos = (prevPos + currPos + nextPos) / 3.0;
            qreal avgPressure = (prevPressure + currPressure + nextPressure) / 3.0;
            
            // Interpolate between original and averaged based on factor
            temp[i].pos = currPos + (avgPos - currPos) * factor;
            temp[i].pressure = currPressure + (avgPressure - currPressure) * factor;
        }
        
        smoothed = temp;
    }
    
    return smoothed;
}

QVector<StrokePoint> CanvasView::generateCurvePoints(const QVector<StrokePoint> &controlPoints) const
{
    if (controlPoints.size() < 2) {
        return controlPoints;
    }
    
    if (controlPoints.size() == 2) {
        // Just interpolate linearly for 2 points
        QVector<StrokePoint> result;
        result.append(controlPoints[0]);
        const StrokePoint &sp0 = controlPoints[0];
        const StrokePoint &sp1 = controlPoints[1];
        qreal dist = QLineF(sp0.pos, sp1.pos).length();
        int steps = qMax(2, (int)(dist / 2.0)); // Sample every ~2 pixels
        for (int i = 1; i < steps; ++i) {
            qreal t = (qreal)i / steps;
            QPointF pos = sp0.pos + (sp1.pos - sp0.pos) * t;
            qreal pressure = sp0.pressure + (sp1.pressure - sp0.pressure) * t;
            result.append(StrokePoint(pos, pressure));
        }
        result.append(controlPoints[1]);
        return result;
    }
    
    // Catmull-Rom spline with dense sampling
    QVector<StrokePoint> curvePoints;
    curvePoints.append(controlPoints[0]);
    
    for (int i = 0; i < controlPoints.size() - 1; ++i) {
        const StrokePoint &sp0 = (i > 0) ? controlPoints[i - 1] : controlPoints[i];
        const StrokePoint &sp1 = controlPoints[i];
        const StrokePoint &sp2 = controlPoints[i + 1];
        const StrokePoint &sp3 = (i + 2 < controlPoints.size()) ? controlPoints[i + 2] : sp2;
        
        // Calculate segment length for adaptive sampling
        qreal segLength = QLineF(sp1.pos, sp2.pos).length();
        int steps = qMax(2, (int)(segLength / 2.0)); // Sample every ~2 pixels
        
        // Generate points along Catmull-Rom curve
        for (int j = 1; j <= steps; ++j) {
            qreal t = (qreal)j / steps;
            qreal t2 = t * t;
            qreal t3 = t2 * t;
            
            // Catmull-Rom basis functions
            qreal q0 = -t3 + 2.0 * t2 - t;
            qreal q1 = 3.0 * t3 - 5.0 * t2 + 2.0;
            qreal q2 = -3.0 * t3 + 4.0 * t2 + t;
            qreal q3 = t3 - t2;
            
            // Interpolate position
            QPointF pt;
            pt.setX(0.5 * (sp0.pos.x() * q0 + sp1.pos.x() * q1 + sp2.pos.x() * q2 + sp3.pos.x() * q3));
            pt.setY(0.5 * (sp0.pos.y() * q0 + sp1.pos.y() * q1 + sp2.pos.y() * q2 + sp3.pos.y() * q3));
            
            // Interpolate pressure
            qreal pressure = 0.5 * (sp0.pressure * q0 + sp1.pressure * q1 + sp2.pressure * q2 + sp3.pressure * q3);
            
            curvePoints.append(StrokePoint(pt, pressure));
        }
    }
    
    return curvePoints;
}



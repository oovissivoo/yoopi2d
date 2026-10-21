#include "canvas.h"
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRgb>
#include <QStack>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QtMath>
#include <QVector2D>
#include <QStyleOptionGraphicsItem>
#include <QImageWriter>
#include <QGraphicsScene>
#include <QQueue>
#include <QVector>
#include <QTransform>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsView>
#include <QTabletEvent>
#include <QDateTime>
#include <QPainterPath>
#include <QLineF>
#include <QPainterPathStroker>
#include <cmath>

Canvas::Canvas(QGraphicsItem *parent)
    : QObject(), QGraphicsItem(parent)
    , m_color(Qt::black)
    , m_lineColor(Qt::transparent)
    , m_brushSize(10)
    , m_opacity(100)
    , m_fillMode(false)
    , m_contiguousSelection(true)
    , m_autoLineFill(false)
    , m_drawing(false)
    , m_useTablet(true)
    , m_tabletActive(false)
    , m_lastPressure(1.0)
    , m_lastTiltX(0.0)
    , m_lastTiltY(0.0)
    , m_lastRotation(0.0)
    , m_isPanning(false)
    , m_isSpacePressed(false)
    , m_isCtrlPressed(false)
    , m_currentScale(1.0)
    , m_colorSamplingMode(false)
    , m_colorReplaceMode(false)
    , m_customCursorEnabled(true)
    , m_rectGapFillMode(false)
    , m_lassoGapFillMode(false)
    , m_lassoUseActiveColor(false)
    , m_isSelecting(false)
    , m_selectStart()
    , m_selectCurrent()
    , m_lassoPath()
    , m_sampledFillColor()
{
    qDebug() << "Canvas constructor called";
    
    // Create a white canvas
    m_image = QImage(800, 600, QImage::Format_ARGB32);
    m_image.fill(Qt::white);
    
    // Enable hover events
    setAcceptHoverEvents(true);
    
    qDebug() << "Initializing brush engine";
    // Initialize brush engine
    m_brushEngine = new BrushEngine(this);
    
    // Enable hover events to track mouse movement without clicking
    setAcceptHoverEvents(true);
    
    // Enable mouse tracking
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    
    // Accept keyboard focus
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    
    // Accept tablet events
    setAcceptedMouseButtons(Qt::AllButtons);
    
    // Set transformation settings
    setTransformOriginPoint(boundingRect().center());
    
    // Enable custom cursor by default - this hides the system cursor
    if (m_customCursorEnabled) {
        // Hide the system cursor
        setCursor(Qt::BlankCursor);
        
        // Initialize position history with current position to avoid initial jitter
        m_cursorPos = QPointF(m_image.width() / 2, m_image.height() / 2);
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        
        // Populate initial history to avoid issues with predictive calculations
        for (int i = 0; i < POSITION_HISTORY_SIZE; i++) {
            m_positionHistory.append(m_cursorPos);
            m_timeHistory.append(currentTime - (POSITION_HISTORY_SIZE - i));
        }
    }
    
    qDebug() << "Canvas constructor complete";
    
    // Save initial state
    saveState();
    
    // Initialize signals for undo/redo
    emit undoAvailable(false);
    emit redoAvailable(false);

    // Initialize marching ants timer
    m_antsTimer = new QTimer(this);
    m_antsTimer->setInterval(100);
    connect(m_antsTimer, &QTimer::timeout, this, [this]() {
        m_antsOffset = std::fmod(m_antsOffset + 1.0, 12.0);
        if (m_selectionActive) update();
    });

    // Navigation flush timer: caps pan/zoom updates to TARGET_NAV_FPS.
    // High-frequency tablet events accumulate into m_pendingPanDelta and are
    // applied once per timer tick instead of once per raw event.
    m_navFlushTimer = new QTimer(this);
    m_navFlushTimer->setInterval(1000 / TARGET_NAV_FPS);
    m_navFlushTimer->setSingleShot(false);
    connect(m_navFlushTimer, &QTimer::timeout, this, &Canvas::flushPendingNavigation);
}

void Canvas::setSize(const QSize &size)
{
    m_image = QImage(size, QImage::Format_ARGB32);
    m_image.fill(Qt::transparent);
    
    // Clear undo/redo stacks when creating a new canvas
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoAvailable(false);
    emit redoAvailable(false);
    
    update();
}

void Canvas::setColor(const QColor &color)
{
    m_color = color;
    
    // Update brush engine color
    BrushSettings settings = m_brushEngine->settings();
    settings.color = color;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushSize(int size)
{
    m_brushSize = size;
    
    // Update brush engine size
    BrushSettings settings = m_brushEngine->settings();
    settings.size = size;
    m_brushEngine->setSettings(settings);
}

void Canvas::setOpacity(int opacity)
{
    m_opacity = opacity;
    
    // Update brush engine opacity
    BrushSettings settings = m_brushEngine->settings();
    settings.opacity = opacity / 255.0;
    m_brushEngine->setSettings(settings);
}

void Canvas::setFillMode(bool fill)
{
    m_fillMode = fill;
}

void Canvas::setContiguousSelection(bool contiguous)
{
    m_contiguousSelection = contiguous;
}

void Canvas::setAutoLineFill(bool enable)
{
    m_autoLineFill = enable;
}

void Canvas::setLineColor(const QColor &color)
{
    m_lineColor = color;
    // Also add to the line colors list if not already present
    if (!m_lineColors.contains(color)) {
        m_lineColors.append(color);
    }
}

void Canvas::addLineColor(const QColor &color)
{
    if (!m_lineColors.contains(color)) {
        m_lineColors.append(color);
    }
}

void Canvas::clearLineColors()
{
    m_lineColors.clear();
}

bool Canvas::isLineColor(const QRgb &color) const
{
    // Check if the color is in our line colors list
    for (const QColor &lineColor : m_lineColors) {
        if (lineColor.rgba() == color) {
            return true;
        }
    }
    return false;
}

QImage Canvas::getImage() const
{
    return m_image;
}

void Canvas::setImage(const QImage &newImage)
{
    if (newImage.size() != m_image.size()) {
        prepareGeometryChange();
        m_image = newImage;
    } else {
        m_image = newImage;
    }
    update();
    emit imageChanged();
}

void Canvas::clear()
{
    m_image.fill(Qt::transparent);
    update();
    emit imageChanged();
}

QRectF Canvas::boundingRect() const
{
    return QRectF(0, 0, m_image.width(), m_image.height());
}

void Canvas::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    
    // Set clipping for better performance - paint only what's visible
    painter->setClipRect(option->exposedRect);
    
    // Draw background color 
    painter->fillRect(boundingRect(), QColor("#303030"));
    
    // Draw the high-resolution image directly
    painter->drawImage(QPointF(0, 0), m_image);

    // Draw floating selection if any
    if (m_hasFloating && !m_floatingImage.isNull()) {
        painter->drawImage(m_floatingBounds.topLeft() + m_floatingOffset, m_floatingImage);
    }
    
    // Draw custom cursor if enabled
    if (m_customCursorEnabled) {
        QPointF cursorPos;
        
        // When actually drawing, ALWAYS use the exact cursor position without prediction
        // This ensures we don't see jumps or gaps during drawing
        if (m_drawing) {
            cursorPos = m_cursorPos;
        } else {
            // Only use prediction for non-drawing situations (hover)
            // and only when we have enough history
            if (m_positionHistory.size() >= 2) {
                cursorPos = predictNextCursorPosition();
            } else {
                cursorPos = m_cursorPos;
            }
        }
        
        // Draw the custom cursor at the current or predicted position
        drawCustomCursor(painter, cursorPos, m_brushSize);
    }

    // New: draw selection gizmos for gap fill tools
    if (m_rectGapFillMode && m_isSelecting) {
        drawRectSelectionGizmo(painter);
    }
    if (m_lassoGapFillMode && m_isSelecting && !m_lassoPath.isEmpty()) {
        drawLassoGizmo(painter);
    }

    // Selection overlay (marquee outline and mask tint)
    if (m_selectionActive) {
        drawSelectionOverlay(painter);
    }
}

void Canvas::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // Skip mouse events if tablet is active
    if (m_tabletActive) return;
    
    if (event->button() == Qt::LeftButton) {
        // Selection tools take precedence
        if (m_rectSelectMode || m_lassoSelectMode) {
            QPointF cpos = event->pos();

            // If clicking inside an existing selection and floating exists, start dragging
            if (m_selectionActive && (m_hasFloating || pointInSelection(cpos))) {
                m_draggingSelection = true;
                m_dragStartPos = cpos;
                m_dragStartOffset = m_floatingOffset;
                if (!m_hasFloating) {
                    // Extract floating as copy by default when starting drag
                    extractFloatingFromSelection(false);
                }
                update();
                return;
            }

            // Begin a new selection
            m_isSelecting = true;
            if (m_rectSelectMode) {
                beginRectSelection(cpos);
            } else if (m_lassoSelectMode) {
                beginLassoSelection(cpos);
            }
            update();
            return;
        }

        // Gap fill rectangle mode
        if (m_rectGapFillMode) {
            m_isSelecting = true;
            m_selectStart = event->pos();
            m_selectCurrent = m_selectStart;
            // Sample starting color under cursor if not lasso active color mode
            m_sampledFillColor = QColor::fromRgba(m_image.pixel(m_selectStart.toPoint()));
            update();
            return;
        }
        // Gap fill lasso mode
        if (m_lassoGapFillMode) {
            m_isSelecting = true;
            m_lassoPath = QPainterPath(event->pos());
            // Sample color unless we are configured to use active color
            if (!m_lassoUseActiveColor) {
                m_sampledFillColor = QColor::fromRgba(m_image.pixel(event->pos().toPoint()));
            }
            update();
            return;
        }
        
        if (m_colorSamplingMode) {
            // In color sampling mode, only emit the signal and do nothing else
            emit canvasClicked(event->scenePos());
            return;
        }
        
        // Not in color sampling mode - proceed with normal drawing operations
        if (m_isSpacePressed) {
            // Start panning
            m_isPanning = true;
            m_panStartPos = event->scenePos();
            setCursor(QCursor(Qt::ClosedHandCursor));
        } else {
            // Only save state if we have a valid image and we're actually going to modify it
            if (!m_image.isNull() && m_image.width() > 0 && m_image.height() > 0) {
                saveState();
            }
            
            // Normal drawing operations
            m_drawing = true;
            m_lastPos = event->pos();
            
            if (m_fillMode) {
                fillArea(event->pos());
            } else if (m_contiguousSelection) {
                contiguousSelection(event->pos());
            } else {
                // Start a brush stroke
                StrokePoint point = convertToStrokePoint(event->pos());
                
                m_brushEngine->beginStroke(point);
                m_brushEngine->applyDab(m_image, point);
            }
            
            update();
            
            // Emit signal
            emit canvasClicked(event->scenePos());
        }
    }
    else if (event->button() == Qt::RightButton) {
        // Emit signal for right-click
        emit canvasRightClicked(event->scenePos());
    }
}

void Canvas::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // Skip mouse events if tablet is active
    if (m_tabletActive) return;

    // Update selection drag in selection modes
    if (m_isSelecting && (m_rectSelectMode || m_lassoSelectMode)) {
        if (m_rectSelectMode) {
            updateRectSelection(event->pos());
        } else {
            updateLassoSelection(event->pos());
        }
        update();
        return;
    }

    if (m_draggingSelection && m_selectionActive) {
        QPointF delta = event->pos() - m_dragStartPos;
        m_floatingOffset = m_dragStartOffset + delta;
        update();
        return;
    }

    // Update selection drag in gap fill modes
    if (m_isSelecting) {
        if (m_rectGapFillMode) {
            m_selectCurrent = event->pos();
            update();
            return;
        }
        if (m_lassoGapFillMode) {
            QPointF p = event->pos();
            if (QLineF(m_lassoPath.currentPosition(), p).length() > 0.5) {
                m_lassoPath.lineTo(p);
                update();
            }
            return;
        }
    }
    
    if (m_colorSamplingMode) {
        // Convert hover position to scene position and emit preview
        emit canvasMouseMoved(event->scenePos());
        return;
    }
    
    if (m_isPanning && m_isSpacePressed) {
        // Accumulate pan delta instead of applying every raw event.
        // The nav flush timer will apply it at a capped rate (TARGET_NAV_FPS),
        // so thousands of tablet events per second collapse into ~60 transforms.
        QPointF delta = event->scenePos() - m_panStartPos;
        m_pendingPanDelta += delta;
        m_panStartPos = event->scenePos();
        m_hasPendingPan = true;
        if (!m_navFlushTimer->isActive()) {
            m_navFlushTimer->start();
        }
    } else if (m_drawing && !m_fillMode && !m_contiguousSelection) {
        // Using the brush engine for drawing
        StrokePoint point = convertToStrokePoint(event->pos());
        
        m_brushEngine->continueStroke(m_image, point);
        
        update();
        
        // Also emit the canvasMouseMoved signal
        emit canvasMouseMoved(event->scenePos());
    }
}

void Canvas::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // Skip mouse events if tablet is active
    if (m_tabletActive) return;
    
    if (event->button() == Qt::LeftButton) {
        // Finalize selection creation
        if (m_isSelecting && (m_rectSelectMode || m_lassoSelectMode)) {
            finalizeSelection();
            m_isSelecting = false;
            update();
            return;
        }

        // Stop dragging selection
        if (m_draggingSelection) {
            m_draggingSelection = false;
            update();
            return;
        }

        // Commit gap fill on release
        if (m_isSelecting && (m_rectGapFillMode || m_lassoGapFillMode)) {
            // Save for undo
            saveState();
            QColor fillColor = (m_lassoGapFillMode && m_lassoUseActiveColor) ? m_color : m_sampledFillColor;
            if (m_rectGapFillMode) {
                QRect rect = QRect(m_selectStart.toPoint(), m_selectCurrent.toPoint()).normalized();
                rect = rect.intersected(QRect(0, 0, m_image.width(), m_image.height()));
                if (rect.isValid() && !rect.isEmpty()) {
                    fillTransparentInRect(rect, fillColor);
                }
            } else if (m_lassoGapFillMode) {
                QPainterPath closed = m_lassoPath;
                closed.closeSubpath();
                // Bounds safety
                fillTransparentInPath(closed, fillColor);
            }
            // Reset selection state
            m_isSelecting = false;
            m_lassoPath = QPainterPath();
            update();
            emit frameUpdated(m_image);
            return;
        }
        
        if (m_isPanning) {
            // Flush any remaining accumulated delta before stopping
            flushPendingNavigation();
            m_isPanning = false;
            setCursor(QCursor(Qt::ArrowCursor));
        }
        
        if (m_drawing && !m_fillMode && !m_contiguousSelection) {
            m_brushEngine->endStroke(m_image);
            m_drawing = false;
            update();
            
            // Emit frame update signal when drawing is complete
            emit frameUpdated(m_image);
        }
    }
}

void Canvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space) {
        m_isSpacePressed = true;
        if (!m_isPanning && !m_isCtrlPressed) {
            setCursor(QCursor(Qt::OpenHandCursor));
        }
    } else if (event->key() == Qt::Key_Control) {
        m_isCtrlPressed = true;
    }
    if (event->key() == Qt::Key_Escape) {
        // Cancel selection modes or dragging
        if (m_draggingSelection) {
            m_draggingSelection = false;
            m_floatingOffset = m_dragStartOffset;
            update();
            return;
        }
        if (m_isSelecting) {
            m_isSelecting = false;
            m_selectionPath = QPainterPath();
            update();
            return;
        }
    }
    if (m_selectionActive) {
        if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
            // Clear contents of selection on base image (if not floating) or clear floating
            if (m_hasFloating) {
                // Discard floating image
                m_hasFloating = false;
                m_floatingImage = QImage();
                m_floatingBounds = QRect();
                m_floatingOffset = QPointF(0,0);
                update();
            } else {
                // Erase selected area from base image
                saveState();
                QRect bounds = m_selectionPath.boundingRect().toAlignedRect().intersected(QRect(0,0,m_image.width(), m_image.height()));
                if (bounds.isValid() && !bounds.isEmpty()) {
                    QImage mask(bounds.size(), QImage::Format_Alpha8);
                    mask.fill(0);
                    QPainter pm(&mask);
                    pm.setRenderHint(QPainter::Antialiasing, true);
                    pm.translate(-bounds.topLeft());
                    pm.fillPath(m_selectionPath, Qt::white);
                    pm.end();
                    for (int y = 0; y < bounds.height(); ++y) {
                        const uchar* mrow = mask.constScanLine(y);
                        QRgb* src = reinterpret_cast<QRgb*>(m_image.scanLine(bounds.top() + y)) + bounds.left();
                        for (int x = 0; x < bounds.width(); ++x) {
                            if (mrow[x]) src[x] = 0x00000000;
                        }
                    }
                    emit frameUpdated(m_image);
                }
                update();
            }
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
            // Commit floating back to image
            commitSelection();
            event->accept();
            return;
        }
        if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_C) {
            // Copy selection into floating (do not cut)
            if (!m_hasFloating) extractFloatingFromSelection(false);
            event->accept();
            update();
            return;
        }
        if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_X) {
            // Cut selection into floating
            if (!m_hasFloating) extractFloatingFromSelection(true);
            event->accept();
            update();
            return;
        }
    }
    QGraphicsItem::keyPressEvent(event);
}

void Canvas::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space) {
        m_isSpacePressed = false;
        if (!m_isPanning) {
            setCursor(QCursor(Qt::ArrowCursor));
        }
    } else if (event->key() == Qt::Key_Control) {
        m_isCtrlPressed = false;
    }
    QGraphicsItem::keyReleaseEvent(event);
}

void Canvas::flushPendingNavigation()
{
    // --- Pan ---
    if (m_hasPendingPan) {
        setPos(pos() + m_pendingPanDelta);
        m_pendingPanDelta = QPointF(0, 0);
        m_hasPendingPan = false;
    }

    // --- Zoom ---
    if (m_hasPendingZoom) {
        if (scene() && !scene()->views().isEmpty()) {
            QGraphicsView *view = scene()->views().first();
            // Apply accumulated factor to tracked scale
            m_currentScale *= m_pendingZoomFactor;
            m_currentScale = qBound(0.1, m_currentScale, 10.0);

            // Use setTransform with an absolute scale matrix instead of
            // view->scale() which compounds and causes floating-point drift
            // at high zoom levels, making each repaint progressively heavier.
            QTransform t;
            t.scale(m_currentScale, m_currentScale);
            view->setTransform(t);
        }
        m_pendingZoomFactor = 1.0;
        m_hasPendingZoom = false;
    }

    // Stop timer if no active navigation
    if (!m_isPanning && !m_hasPendingPan && !m_hasPendingZoom) {
        m_navFlushTimer->stop();
    }
}

void Canvas::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    // Calculate zoom factor for this tick
    qreal factor = event->delta() > 0 ? 1.1 : 0.9;
    qreal newScale = m_currentScale * m_pendingZoomFactor * factor;

    // Clamp to zoom range before accepting
    if (newScale < 0.1 || newScale > 10.0) {
        event->accept();
        return;
    }

    // Accumulate the zoom factor — multiple wheel ticks in one frame
    // collapse into a single setTransform() call on the next flush.
    m_pendingZoomFactor *= factor;
    m_hasPendingZoom = true;

    if (!m_navFlushTimer->isActive()) {
        m_navFlushTimer->start();
    }

    event->accept();
}

void Canvas::drawLine(const QPointF &start, const QPointF &end)
{
    // Use the brush engine to draw a stroke between two points
    StrokePoint startPoint = convertToStrokePoint(start);
    StrokePoint endPoint = convertToStrokePoint(end);
    
    m_brushEngine->drawStroke(m_image, startPoint, endPoint);
    
    // Update the canvas
    update();
}

// Convert a QPointF position to a StrokePoint with all tablet data
StrokePoint Canvas::convertToStrokePoint(const QPointF &pos, 
                                         qreal pressure,
                                         qreal tiltX,
                                         qreal tiltY, 
                                         qreal rotation) const
{
    StrokePoint point(pos);
    
    // Set tablet data
    point.pressure = pressure;
    point.tiltX = tiltX;
    point.tiltY = tiltY;
    point.rotation = rotation;
    
    // Calculate speed (can be implemented if needed)
    // For now, we'll just set a default value
    point.speed = 0.0;
    
    return point;
}

// Implement new brush engine settings methods
void Canvas::setBrushHardness(qreal hardness)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.hardness = hardness;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushFlow(qreal flow)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.flow = flow;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushRoundness(qreal roundness)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.roundness = roundness;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushAngle(qreal angle)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.angle = angle;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushSpacing(qreal spacing)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.spacing = spacing;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushJitter(qreal jitter)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.jitter = jitter;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushScatter(qreal scatterX, qreal scatterY)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.scatterX = scatterX;
    settings.scatterY = scatterY;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushTip(BrushTip tip)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.brushTip = tip;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushTexture(const QString &texturePath)
{
    BrushSettings settings = m_brushEngine->settings();
    if (!texturePath.isEmpty()) {
        settings.loadTexture(texturePath);
        settings.brushTip = BrushTip::Texture;
    }
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushBlendMode(BlendMode mode)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.blendMode = mode;
    m_brushEngine->setSettings(settings);
}

void Canvas::setBrushPressureEnabled(bool enabled, bool sizeEnabled, bool opacityEnabled)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.usePressure = enabled;
    settings.useSizePressure = sizeEnabled;
    settings.useOpacityPressure = opacityEnabled;
    m_brushEngine->setSettings(settings);
}

void Canvas::setAntialiasingEnabled(bool enabled)
{
    BrushSettings settings = m_brushEngine->settings();
    settings.antialias = enabled;
    m_brushEngine->setSettings(settings);
}

BrushEngine* Canvas::brushEngine() const
{
    return m_brushEngine;
}

void Canvas::fillArea(const QPointF &pos)
{
    if (!isValidPosition(pos)) {
        return;
    }
    
    // Save current state for undo
    saveState();
    
    QPoint pixelPos = pos.toPoint();
    QRgb targetColor = m_image.pixel(pixelPos);
    QRgb fillColor = qPremultiply(m_color.rgba());
    
    // If target color is the same as fill color, no need to fill
    if (targetColor == fillColor) {
        return;
    }
    
    // Special case: if color replace mode is on, replace all pixels of target color
    if (m_colorReplaceMode) {
        for (int y = 0; y < m_image.height(); ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb*>(m_image.scanLine(y));
            for (int x = 0; x < m_image.width(); ++x) {
                if (scanLine[x] == targetColor) {
                    scanLine[x] = fillColor;
                }
            }
        }
        // Update canvas
        update();
        
        // Notify that undo is available
        emit undoAvailable(true);
        return;
    }
    
    // Use a more efficient data structure for the fill algorithm
    // Use QVector instead of QList for better performance
    QVector<QPoint> stack;
    stack.reserve(m_image.width() * m_image.height() / 4); // Pre-allocate memory
    
    // Use a QVector-based mask for faster lookups
    QVector<bool> visited(m_image.width() * m_image.height(), false);
    
    // Function to calculate the index in the visited array
    auto getIndex = [this](int x, int y) -> int {
        return y * m_image.width() + x;
    };
    
    // Start with the clicked point
    stack.append(pixelPos);
    visited[getIndex(pixelPos.x(), pixelPos.y())] = true;
    
    // Implement a more efficient scanline flood fill algorithm
    while (!stack.isEmpty()) {
        QPoint current = stack.takeLast();
        int x = current.x();
        int y = current.y();
        
        // Find the leftmost pixel of the scanline
        int left = x;
        while (left > 0 && 
               m_image.pixel(left-1, y) == targetColor && 
               !visited[getIndex(left-1, y)]) {
            left--;
            visited[getIndex(left, y)] = true;
        }
        
        // Find the rightmost pixel of the scanline
        int right = x;
        while (right < m_image.width()-1 && 
               m_image.pixel(right+1, y) == targetColor && 
               !visited[getIndex(right+1, y)]) {
            right++;
            visited[getIndex(right, y)] = true;
        }
        
        // Fill the scanline
        QRgb *scanLine = reinterpret_cast<QRgb*>(m_image.scanLine(y));
        for (int i = left; i <= right; i++) {
            scanLine[i] = fillColor;
        }
        
        // Add scanlines above and below to the stack if they have pixels to fill
        for (int i = left; i <= right; i++) {
            // Check scanline above
            if (y > 0) {
                int index = getIndex(i, y-1);
                if (m_image.pixel(i, y-1) == targetColor && !visited[index]) {
                    stack.append(QPoint(i, y-1));
                    visited[index] = true;
                }
            }
            
            // Check scanline below
            if (y < m_image.height()-1) {
                int index = getIndex(i, y+1);
                if (m_image.pixel(i, y+1) == targetColor && !visited[index]) {
                    stack.append(QPoint(i, y+1));
                    visited[index] = true;
                }
            }
        }
    }
    
    // Process line pixels if AutoLine fill is enabled
    if (m_autoLineFill) {
        // Create a separate stack for line pixels
        QVector<QPoint> lineStack;
        lineStack.reserve(m_image.width() * m_image.height() / 10);
        
        // Find all line pixels adjacent to filled areas
        for (int y = 0; y < m_image.height(); y++) {
            for (int x = 0; x < m_image.width(); x++) {
                if (visited[getIndex(x, y)]) {
                    // Check 8-connected neighbors for line pixels
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            
                            int nx = x + dx;
                            int ny = y + dy;
                            
                            if (nx >= 0 && nx < m_image.width() && 
                                ny >= 0 && ny < m_image.height() &&
                                !visited[getIndex(nx, ny)] &&
                                isLineColor(m_image.pixel(nx, ny))) {
                                lineStack.append(QPoint(nx, ny));
                                visited[getIndex(nx, ny)] = true;
                            }
                        }
                    }
                }
            }
        }
        
        // Process all line pixels
        while (!lineStack.isEmpty()) {
            QPoint current = lineStack.takeLast();
            int x = current.x();
            int y = current.y();
            
            // Fill this line pixel
            m_image.setPixel(x, y, fillColor);
            
            // Check neighbors for more line pixels
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < m_image.width() && 
                        ny >= 0 && ny < m_image.height() &&
                        !visited[getIndex(nx, ny)] &&
                        isLineColor(m_image.pixel(nx, ny))) {
                        lineStack.append(QPoint(nx, ny));
                        visited[getIndex(nx, ny)] = true;
                    }
                }
            }
        }
    }
    
    // Update canvas
    update();
    
    // Notify that undo is available
    emit undoAvailable(true);
}

void Canvas::contiguousSelection(const QPointF &pos)
{
    if (!isValidPosition(pos)) {
        return;
    }
    
    QColor targetColor = m_image.pixelColor(pos.toPoint());
    QVector<QPoint> selected;
    
    QQueue<QPoint> queue;
    QVector<QPoint> visited;
    queue.enqueue(pos.toPoint());
    visited.append(pos.toPoint());
    
    while (!queue.isEmpty()) {
        QPoint current = queue.dequeue();
        selected.append(current);
        
        QPoint neighbors[] = {
            QPoint(current.x() + 1, current.y()),
            QPoint(current.x() - 1, current.y()),
            QPoint(current.x(), current.y() + 1),
            QPoint(current.x(), current.y() - 1)
        };
        
        for (const QPoint &neighbor : neighbors) {
            if (isValidPosition(neighbor) && 
                !visited.contains(neighbor) && 
                m_image.pixelColor(neighbor) == targetColor) {
                queue.enqueue(neighbor);
                visited.append(neighbor);
            }
        }
    }
    
    // Highlight selected area
    QPainter painter(&m_image);
    painter.setCompositionMode(QPainter::CompositionMode_Difference);
    painter.setPen(Qt::white);
    painter.setBrush(Qt::transparent);
    
    for (const QPoint &point : selected) {
        painter.drawRect(point.x() - 1, point.y() - 1, 2, 2);
    }
    
    // Update canvas
    update();
}

bool Canvas::isValidPosition(const QPointF &pos) const
{
    return pos.x() >= 0 && pos.x() < m_image.width() &&
           pos.y() >= 0 && pos.y() < m_image.height();
}

void Canvas::setColorSamplingMode(bool enabled)
{
    m_colorSamplingMode = enabled;
}

void Canvas::setColorReplaceMode(bool enable)
{
    m_colorReplaceMode = enable;
}

// New: setters for gap fill tools
void Canvas::setRectGapFillMode(bool enabled)
{
    if (m_rectGapFillMode == enabled) return;
    m_rectGapFillMode = enabled;
    if (enabled) {
        // disable other transient modes
        m_lassoGapFillMode = false;
        m_isSelecting = false;
        m_lassoPath = QPainterPath();
    }
    update();
}

void Canvas::setLassoGapFillMode(bool enabled)
{
    if (m_lassoGapFillMode == enabled) return;
    m_lassoGapFillMode = enabled;
    if (enabled) {
        // disable other transient modes
        m_rectGapFillMode = false;
        m_isSelecting = false;
        m_lassoPath = QPainterPath();
    }
    update();
}

void Canvas::setLassoUseActiveColor(bool enabled)
{
    m_lassoUseActiveColor = enabled;
}

// Add the new undo/redo methods
bool Canvas::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool Canvas::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void Canvas::undo()
{
    if (canUndo()) {
        // Save current state to redo stack
        m_redoStack.push(m_image.copy());
        
        // Restore previous state
        m_image = m_undoStack.pop();
        
        // Update signal states
        emit undoAvailable(canUndo());
        emit redoAvailable(true);
        
        update();
    }
}

void Canvas::redo()
{
    if (canRedo()) {
        // Save current state to undo stack
        m_undoStack.push(m_image.copy());
        
        // Restore redone state
        m_image = m_redoStack.pop();
        
        // Update signal states
        emit undoAvailable(true);
        emit redoAvailable(canRedo());
        
        update();
    }
}

void Canvas::saveState()
{
    // Don't save state if the image is empty or null
    if (m_image.isNull() || (m_image.width() == 0 || m_image.height() == 0)) {
        return;
    }

    // Add current state to undo stack
    m_undoStack.push(m_image.copy());
    
    // Limit undo stack size
    while (m_undoStack.size() > MAX_UNDO_STEPS) {
        m_undoStack.removeFirst();
    }
    
    // Clear redo stack since a new action was performed
    clearRedoStack();
    
    // Update signals
    emit undoAvailable(true);
    emit redoAvailable(false);
}

void Canvas::clearRedoStack()
{
    m_redoStack.clear();
    emit redoAvailable(false);
}

void Canvas::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    // Make sure system cursor is hidden
    if (m_customCursorEnabled) {
        setCursor(Qt::BlankCursor);
        
        // Also set cursor for parent view/widget
        if (scene() && !scene()->views().isEmpty()) {
            foreach (QGraphicsView* view, scene()->views()) {
                view->setCursor(Qt::BlankCursor);
                view->viewport()->setCursor(Qt::BlankCursor);
            }
        }
    }
    
    // Update cursor position with the same consistent approach
    if (m_customCursorEnabled) {
        m_cursorPos = event->pos();
        
        // Update position history
        m_positionHistory.append(m_cursorPos);
        m_timeHistory.append(QDateTime::currentMSecsSinceEpoch());
        
        // Keep history size reasonable
        while (m_positionHistory.size() > POSITION_HISTORY_SIZE) {
            m_positionHistory.removeFirst();
            m_timeHistory.removeFirst();
        }
        
        // Force update to redraw cursor
        update();
    }
    
    if (m_colorSamplingMode) {
        // Convert hover position to scene position
        QPointF scenePos = mapToScene(event->pos());
        emit canvasMouseMoved(scenePos);
    }
    
    QGraphicsItem::hoverMoveEvent(event);
}

// Fix the sceneEvent method
bool Canvas::sceneEvent(QEvent *event)
{
    qDebug() << "Canvas sceneEvent:" << event->type();
    
    // Check if the event is a tablet event
    if (event->type() == QEvent::TabletPress ||
        event->type() == QEvent::TabletMove ||
        event->type() == QEvent::TabletRelease) {
        
        qDebug() << "Processing tablet event";
        
        // Call the tabletEvent handler
        this->tabletEvent(static_cast<QTabletEvent *>(event));
        return true;
    }
    
    // Let the parent class handle other event types
    return QGraphicsItem::sceneEvent(event);
}

void Canvas::tabletEvent(QTabletEvent *event)
{
    qDebug() << "tabletEvent called, type:" << event->type();
    
    // Accept the event first to ensure proper handling
    event->accept();
    
    // Convert the tablet position to scene coordinates
    QPointF pos = event->pos();
    
    // Make sure system cursor is hidden
    if (m_customCursorEnabled) {
        setCursor(Qt::BlankCursor);
        
        // Also set cursor for parent view/widget
        if (scene() && !scene()->views().isEmpty()) {
            foreach (QGraphicsView* view, scene()->views()) {
                view->setCursor(Qt::BlankCursor);
                view->viewport()->setCursor(Qt::BlankCursor);
            }
        }
    }
    
    // IMPORTANT: Always update cursor position immediately
    // This is crucial for preventing cursor jumps during drawing
    if (m_customCursorEnabled) {
        m_cursorPos = pos;
        
        // Update position history
        m_positionHistory.append(m_cursorPos);
        m_timeHistory.append(QDateTime::currentMSecsSinceEpoch());
        
        // Only keep a reasonable amount of history
        // Too much history can cause lag in the calculations
        while (m_positionHistory.size() > POSITION_HISTORY_SIZE) {
            m_positionHistory.removeFirst();
            m_timeHistory.removeFirst();
        }
    }
    
    qDebug() << "Tablet position:" << pos << "Pressure:" << event->pressure();
    
    // Get pressure, tilt, and rotation data
    qreal pressure = event->pressure();
    qreal tiltX = event->xTilt();
    qreal tiltY = event->yTilt();
    qreal rotation = event->rotation();
    
    // Check if the tablet is active and using a stylus
    bool isStylus = (event->pointerType() == QTabletEvent::Pen || 
                     event->pointerType() == QTabletEvent::Eraser);
    
    // Only use tablet if pressure sensitivity is enabled and we have a stylus
    BrushSettings settings = m_brushEngine->settings();
    bool pressureEnabled = settings.usePressure && 
                          (settings.useSizePressure || settings.useOpacityPressure);
    
    // Force pressure usage if we have a valid pressure reading from a stylus
    if (!pressureEnabled || !isStylus || pressure <= 0.0) {
        pressure = 1.0;
    }
    
    // Store the current tablet state
    m_tabletActive = true;
    m_lastPressure = pressure;
    m_lastTiltX = tiltX;
    m_lastTiltY = tiltY;
    m_lastRotation = rotation;
    
    if (event->type() == QEvent::TabletPress) {
        if (isValidPosition(pos)) {
            m_drawing = true;
            m_lastPos = pos;
            
            // Create a stroke point with all tablet data
            StrokePoint point = convertToStrokePoint(pos, pressure, tiltX, tiltY, rotation);
            
            // Save state for undo
            saveState();
            
            // Start a new stroke with the brush engine
            m_brushEngine->beginStroke(point);
            m_brushEngine->applyDab(m_image, point);
            
            // Update the canvas
            update();
        }
    } 
    else if (event->type() == QEvent::TabletMove && m_drawing) {
        if (isValidPosition(pos)) {
            // Create a stroke point with all tablet data
            StrokePoint point = convertToStrokePoint(pos, pressure, tiltX, tiltY, rotation);
            
            // Continue the stroke
            m_brushEngine->continueStroke(m_image, point);
            
            // Update the last position
            m_lastPos = pos;
            
            // Update the canvas
            update();
        }
    } 
    else if (event->type() == QEvent::TabletRelease) {
        if (m_drawing) {
            // End the stroke
            m_brushEngine->endStroke(m_image);
            
            // Reset drawing state
            m_drawing = false;
            
            // Update the canvas
            update();
        }
        
        // Reset tablet active flag when stylus is lifted
        m_tabletActive = false;
    }
}

void Canvas::setCustomCursorEnabled(bool enabled)
{
    m_customCursorEnabled = enabled;
    
    // When enabling/disabling custom cursor, update cursor visibility
    if (m_customCursorEnabled) {
        setCursor(Qt::BlankCursor);
        
        // Ensure the scene/view also doesn't override our cursor
        if (scene() && !scene()->views().isEmpty()) {
            foreach (QGraphicsView* view, scene()->views()) {
                view->setCursor(Qt::BlankCursor);
                view->viewport()->setCursor(Qt::BlankCursor);
            }
        }
    } else {
        setCursor(Qt::ArrowCursor);
        
        // Reset cursor on scene/view
        if (scene() && !scene()->views().isEmpty()) {
            foreach (QGraphicsView* view, scene()->views()) {
                view->setCursor(Qt::ArrowCursor);
                view->viewport()->setCursor(Qt::ArrowCursor);
            }
        }
    }
    
    // Force update to show/hide custom cursor
    update();
}

bool Canvas::isCustomCursorEnabled() const
{
    return m_customCursorEnabled;
}

void Canvas::drawCustomCursor(QPainter *painter, const QPointF &pos, int size)
{
    if (!m_customCursorEnabled) return;
    
    // Store cursor position for use in other methods
    m_cursorPos = pos;
    
    // Save painter state
    painter->save();
    
    // Enable antialiasing for smoother cursor rendering
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // Calculate cursor size based on brush size and pressure if applicable
    qreal effectiveSize = size;
    if (m_drawing && m_useTablet && m_brushEngine->settings().useSizePressure) {
        effectiveSize *= m_lastPressure;
    }
    
    // Ensure minimum size for visibility
    effectiveSize = qMax(effectiveSize, 4.0);
    
    // Draw cursor with two outlines for better visibility against any background
    
    // Outer outline (black/dark)
    QPen outerPen(QColor(0, 0, 0, 180), 1.5);
    outerPen.setStyle(Qt::SolidLine);
    painter->setPen(outerPen);
    painter->setBrush(Qt::NoBrush);
    
    // Draw outer circle slightly larger
    painter->drawEllipse(pos, effectiveSize / 2.0 + 1.0, effectiveSize / 2.0 + 1.0);
    
    // Inner outline (white/light)
    QPen innerPen;
    if (m_drawing) {
        // Brighter and more opaque when drawing
        innerPen = QPen(QColor(255, 255, 255, 220), 1.0);
    } else {
        // More subtle when not drawing
        innerPen = QPen(QColor(255, 255, 255, 180), 1.0);
    }
    innerPen.setStyle(Qt::SolidLine);
    painter->setPen(innerPen);
    
    // Draw inner circle at exact size
    painter->drawEllipse(pos, effectiveSize / 2.0, effectiveSize / 2.0);
    
    // For drawing, add a center dot to show exact position
    if (m_drawing) {
        // Create a small filled circle at the center
        painter->setBrush(QColor(255, 255, 255, 200));
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(pos, 1.5, 1.5);
    }
    
    // For fast movements, add a motion trail
    if (m_positionHistory.size() >= 3 && m_drawing) {
        // Calculate speed from recent positions
        QPointF delta = m_positionHistory.last() - m_positionHistory[m_positionHistory.size() - 3];
        qreal speed = QVector2D(delta).length();
        
        // Only show trail for fast movements
        if (speed > 3.0) {
            // Get a few previous positions for the trail
            int steps = qMin(5, m_positionHistory.size() - 1);
            
            // Draw trail dots with decreasing opacity
            painter->setBrush(Qt::NoBrush);
            for (int i = 1; i <= steps; i++) {
                int historyIndex = m_positionHistory.size() - 1 - i;
                if (historyIndex >= 0) {
                    QPointF trailPos = m_positionHistory[historyIndex];
                    
                    // Calculate opacity based on position in trail
                    int opacity = 150 - (i * 25);
                    
                    // Draw trail dot
                    painter->setPen(QPen(QColor(255, 255, 255, opacity), 1.0));
                    painter->drawEllipse(trailPos, 1.0, 1.0);
                }
            }
        }
    }
    
    // Restore painter state
    painter->restore();
}

QPointF Canvas::predictNextCursorPosition() const
{
    // If we don't have enough history, return current position
    if (m_positionHistory.size() < 2) {
        return m_cursorPos;
    }
    
    // While drawing, we want to keep the cursor at the exact drawing position
    // This is important to avoid the jumping effect that interrupts the drawing experience
    if (m_drawing) {
        return m_cursorPos;
    }
    
    // For hover (non-drawing) state only, we can use a very subtle prediction
    // that provides a smoother feel without jumping
    
    // Simple velocity calculation based on the most recent positions
    QPointF lastPos = m_positionHistory.last();
    QPointF prevPos = m_positionHistory[m_positionHistory.size() - 2];
    qint64 lastTime = m_timeHistory.last();
    qint64 prevTime = m_timeHistory[m_timeHistory.size() - 2];
    
    qint64 deltaTime = lastTime - prevTime;
    if (deltaTime <= 0) {
        return m_cursorPos;
    }
    
    QPointF velocity = (lastPos - prevPos) / deltaTime;
    
    // Measure how fast the cursor is moving
    qreal speed = QVector2D(velocity).length();
    
    // Only apply prediction for slower movements
    // For fast movements, prediction tends to overshoot and cause jumping
    if (speed > 0.3) {
        return m_cursorPos;
    }
    
    // Very conservative prediction window (much smaller than before)
    qreal predictionTimeMs = 5.0;
    
    // Apply minimal prediction
    return m_cursorPos + (velocity * predictionTimeMs);
}

void Canvas::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_customCursorEnabled) {
        setCursor(Qt::BlankCursor);
        
        // Also set cursor for parent view/widget
        if (scene() && !scene()->views().isEmpty()) {
            foreach (QGraphicsView* view, scene()->views()) {
                view->setCursor(Qt::BlankCursor);
                view->viewport()->setCursor(Qt::BlankCursor);
            }
        }
    }
    
    QGraphicsItem::hoverEnterEvent(event);
}

void Canvas::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_customCursorEnabled) {
        // Restore normal cursor when leaving canvas
        if (scene() && !scene()->views().isEmpty()) {
            foreach (QGraphicsView* view, scene()->views()) {
                view->setCursor(Qt::ArrowCursor);
                view->viewport()->setCursor(Qt::ArrowCursor);
            }
        }
    }
    
    QGraphicsItem::hoverLeaveEvent(event);
} 

// New: helpers
void Canvas::drawRectSelectionGizmo(QPainter* painter) const
{
    QRectF rect(m_selectStart, m_selectCurrent);
    QRectF n = rect.normalized();
    painter->save();
    QPen pen(Qt::white);
    pen.setStyle(Qt::DashLine);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(n);
    painter->restore();
}

void Canvas::drawLassoGizmo(QPainter* painter) const
{
    painter->save();
    QPen pen(Qt::white);
    pen.setStyle(Qt::DashLine);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_lassoPath);
    painter->restore();
}

void Canvas::fillTransparentInRect(const QRect& rect, const QColor& color)
{
    if (!rect.isValid()) return;
    QRgb fill = qPremultiply(color.rgba());
    const int x0 = rect.left();
    const int x1 = rect.right();
    const int y0 = rect.top();
    const int y1 = rect.bottom();
    for (int y = y0; y <= y1; ++y) {
        QRgb* scan = reinterpret_cast<QRgb*>(m_image.scanLine(y));
        for (int x = x0; x <= x1; ++x) {
            QRgb px = scan[x];
            // Only fill fully transparent pixels to close gaps
            if ((px & 0xFF000000) == 0x00000000) {
                scan[x] = fill;
            }
        }
    }
}

void Canvas::fillTransparentInPath(const QPainterPath& path, const QColor& color)
{
    if (path.isEmpty()) return;
    // Create a mask image
    QImage mask(m_image.size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter p(&mask);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillPath(path, Qt::white);
    }
    QRgb fill = qPremultiply(color.rgba());
    for (int y = 0; y < m_image.height(); ++y) {
        const QRgb* mscan = reinterpret_cast<const QRgb*>(mask.constScanLine(y));
        QRgb* iscan = reinterpret_cast<QRgb*>(m_image.scanLine(y));
        for (int x = 0; x < m_image.width(); ++x) {
            // Inside path if mask alpha > 0
            if ((mscan[x] & 0xFF000000) != 0) {
                if ((iscan[x] & 0xFF000000) == 0x00000000) {
                    iscan[x] = fill;
                }
            }
        }
    }
} 

// Selection API
void Canvas::setRectSelectMode(bool enabled)
{
    if (m_rectSelectMode == enabled) return;
    m_rectSelectMode = enabled;
    if (enabled) {
        m_lassoSelectMode = false;
        m_rectGapFillMode = false;
        m_lassoGapFillMode = false;
    }
}

void Canvas::setLassoSelectMode(bool enabled)
{
    if (m_lassoSelectMode == enabled) return;
    m_lassoSelectMode = enabled;
    if (enabled) {
        m_rectSelectMode = false;
        m_rectGapFillMode = false;
        m_lassoGapFillMode = false;
    }
}

void Canvas::clearSelection()
{
    m_selectionActive = false;
    m_selectionPath = QPainterPath();
    m_hasFloating = false;
    m_floatingImage = QImage();
    m_floatingBounds = QRect();
    m_floatingOffset = QPointF(0, 0);
    if (m_antsTimer) m_antsTimer->stop();
    update();
}

void Canvas::commitSelection()
{
    if (!m_selectionActive) return;
    if (m_hasFloating) {
        // Save state for undo
        saveState();
        QPainter p(&m_image);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.drawImage(m_floatingBounds.topLeft() + m_floatingOffset, m_floatingImage);
        p.end();
        emit frameUpdated(m_image);
    }
    clearSelection();
}

void Canvas::drawSelectionOverlay(QPainter* painter) const
{
    if (!m_selectionActive) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPainterPath displayPath = m_selectionPath;
    if (m_hasFloating) {
        QTransform t; t.translate(m_floatingOffset.x(), m_floatingOffset.y());
        displayPath = t.map(displayPath);
    }

    // Semi-transparent tint for selected area when there is no floating image
    if (!m_hasFloating) {
        painter->fillPath(displayPath, QColor(0, 120, 215, 40));
    }

    // Marching ants outline
    QPen pen(Qt::white);
    pen.setCosmetic(true);
    pen.setWidth(1);
    QVector<qreal> pattern { 4.0, 4.0 };
    pen.setDashPattern(pattern);
    pen.setDashOffset(m_antsOffset);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(displayPath);

    // Black offset outline for contrast
    QPen pen2(Qt::black);
    pen2.setCosmetic(true);
    pen2.setWidth(1);
    pen2.setDashPattern(pattern);
    pen2.setDashOffset(m_antsOffset + 4.0);
    painter->setPen(pen2);
    painter->drawPath(displayPath);

    painter->restore();
}

void Canvas::beginRectSelection(const QPointF& start)
{
    m_selectStart = start;
    m_selectCurrent = start;
    m_selectionPath = QPainterPath();
}

void Canvas::updateRectSelection(const QPointF& current)
{
    m_selectCurrent = current;
    QRectF rect(m_selectStart, m_selectCurrent);
    rect = rect.normalized().intersected(boundingRect());
    QPainterPath p; p.addRect(rect);
    m_selectionPath = p;
}

void Canvas::beginLassoSelection(const QPointF& start)
{
    m_lassoPath = QPainterPath(start);
    m_selectionPath = QPainterPath();
}

void Canvas::updateLassoSelection(const QPointF& point)
{
    if (QLineF(m_lassoPath.currentPosition(), point).length() > 0.5) {
        m_lassoPath.lineTo(point);
    }
}

void Canvas::finalizeSelection()
{
    if (m_rectSelectMode) {
        QRectF rect(m_selectStart, m_selectCurrent);
        rect = rect.normalized().intersected(boundingRect());
        QPainterPath p; p.addRect(rect);
        m_selectionPath = p;
    } else if (m_lassoSelectMode) {
        QPainterPath p = m_lassoPath;
        p.closeSubpath();
        m_selectionPath = p;
    }
    m_selectionActive = !m_selectionPath.isEmpty();
    m_lassoPath = QPainterPath();
    if (m_selectionActive && m_antsTimer && !m_antsTimer->isActive()) {
        m_antsTimer->start();
    }
}

bool Canvas::pointInSelection(const QPointF& pos) const
{
    if (!m_selectionActive) return false;
    if (m_hasFloating) {
        QTransform t; t.translate(m_floatingOffset.x(), m_floatingOffset.y());
        return t.map(m_selectionPath).contains(pos);
    }
    return m_selectionPath.contains(pos);
}

void Canvas::extractFloatingFromSelection(bool cut)
{
    if (!m_selectionActive) return;

    // Rasterize selection path to a mask
    QRect bounds = m_selectionPath.boundingRect().toAlignedRect().intersected(QRect(0,0, m_image.width(), m_image.height()));
    if (!bounds.isValid() || bounds.isEmpty()) return;

    QImage mask(bounds.size(), QImage::Format_Alpha8);
    mask.fill(0);
    {
        QPainter pm(&mask);
        pm.setRenderHint(QPainter::Antialiasing, true);
        pm.translate(-bounds.topLeft());
        pm.fillPath(m_selectionPath, Qt::white);
        pm.end();
    }

    // Extract pixels into floatingImage using mask alpha
    QImage floating(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    floating.fill(Qt::transparent);

    for (int y = 0; y < bounds.height(); ++y) {
        const uchar* mrow = mask.constScanLine(y);
        const QRgb* src = reinterpret_cast<const QRgb*>(m_image.constScanLine(bounds.top() + y)) + bounds.left();
        QRgb* dst = reinterpret_cast<QRgb*>(floating.scanLine(y));
        for (int x = 0; x < bounds.width(); ++x) {
            uchar a = mrow[x];
            if (a) {
                QRgb px = src[x];
                // premultiplied result
                dst[x] = px;
            } else {
                dst[x] = 0x00000000;
            }
        }
    }

    if (cut) {
        // Save state for undo
        saveState();
        // Clear source where mask alpha > 0
        for (int y = 0; y < bounds.height(); ++y) {
            const uchar* mrow = mask.constScanLine(y);
            QRgb* src = reinterpret_cast<QRgb*>(m_image.scanLine(bounds.top() + y)) + bounds.left();
            for (int x = 0; x < bounds.width(); ++x) {
                if (mrow[x]) src[x] = 0x00000000;
            }
        }
        emit frameUpdated(m_image);
    }

    m_floatingImage = floating;
    m_floatingBounds = bounds;
    m_floatingOffset = QPointF(0, 0);
    m_hasFloating = true;
} 
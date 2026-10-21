#ifndef CANVAS_H
#define CANVAS_H

#include <QGraphicsItem>
#include <QPainter>
#include <QMouseEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsSceneHoverEvent>
#include <QTabletEvent>
#include <QImage>
#include <QColor>
#include <QObject>
#include <QKeyEvent>
#include <QList>
#include <QStack>
#include <QPainterPath>
#include <QRect>
#include <QTimer>
#include "brushengine.h"

class Canvas : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    explicit Canvas(QGraphicsItem *parent = nullptr);
    void setSize(const QSize &size);
    void setColor(const QColor &color);
    void setBrushSize(int size);
    void setOpacity(int opacity);
    void setFillMode(bool fill);
    void setContiguousSelection(bool contiguous);
    void setAutoLineFill(bool enable);
    void setLineColor(const QColor &color);
    void addLineColor(const QColor &color);
    void clearLineColors();
    bool isLineColor(const QRgb &color) const;
    
    QImage getImage() const;
    void setImage(const QImage &image);
    void clear();
    
    // Add function to set color sampling mode
    void setColorSamplingMode(bool enabled);
    
    // Add method to set color replace mode
    void setColorReplaceMode(bool enable);
    
    // Undo/Redo functions
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    
    // Brush engine settings
    void setBrushHardness(qreal hardness);
    void setBrushFlow(qreal flow);
    void setBrushRoundness(qreal roundness);
    void setBrushAngle(qreal angle);
    void setBrushSpacing(qreal spacing);
    void setBrushJitter(qreal jitter);
    void setBrushScatter(qreal scatterX, qreal scatterY);
    void setBrushTip(BrushTip tip);
    void setBrushTexture(const QString &texturePath);
    void setBrushBlendMode(BlendMode mode);
    void setBrushPressureEnabled(bool enabled, bool sizeEnabled = true, bool opacityEnabled = true);
    void setAntialiasingEnabled(bool enabled);
    
    // Get the brush engine for advanced configurations
    BrushEngine* brushEngine() const;
    
    // Custom cursor methods
    void setCustomCursorEnabled(bool enabled);
    bool isCustomCursorEnabled() const;

    // New: Gap fill tools API
    void setRectGapFillMode(bool enabled);
    void setLassoGapFillMode(bool enabled);
    void setLassoUseActiveColor(bool enabled);
    bool isRectGapFillMode() const { return m_rectGapFillMode; }
    bool isLassoGapFillMode() const { return m_lassoGapFillMode; }
    bool isLassoUseActiveColor() const { return m_lassoUseActiveColor; }

    // New: Selection tools API
    void setRectSelectMode(bool enabled);
    void setLassoSelectMode(bool enabled);
    void clearSelection();
    void commitSelection();
    bool hasSelection() const { return m_selectionActive; }
    
signals:
    void canvasClicked(const QPointF &pos);
    void canvasRightClicked(const QPointF &pos);
    void undoAvailable(bool available);
    void redoAvailable(bool available);
    void canvasMouseMoved(const QPointF &pos);
    void imageChanged();
    void frameUpdated(const QImage &frame);

protected:
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void tabletEvent(QTabletEvent *event);
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    bool sceneEvent(QEvent *event) override;

private:
    void drawLine(const QPointF &start, const QPointF &end);
    void fillArea(const QPointF &pos);
    void contiguousSelection(const QPointF &pos);
    bool isValidPosition(const QPointF &pos) const;
    
    // Save state for undo/redo operations
    void saveState();
    void clearRedoStack();

    // Handles tablet events with pressure
    void handleTabletEvent(QTabletEvent *event);
    
    // Convert mouse/tablet position to a StrokePoint with all tablet data
    StrokePoint convertToStrokePoint(const QPointF &pos, 
                                     qreal pressure = 1.0,
                                     qreal tiltX = 0.0,
                                     qreal tiltY = 0.0,
                                     qreal rotation = 0.0) const;
    
    // Draw custom cursor method
    void drawCustomCursor(QPainter *painter, const QPointF &pos, int size);
    
    // Predict next cursor position based on movement history for lag compensation
    QPointF predictNextCursorPosition() const;

    // New: helpers for gap fill tools
    void drawRectSelectionGizmo(QPainter* painter) const;
    void drawLassoGizmo(QPainter* painter) const;
    void fillTransparentInRect(const QRect& rect, const QColor& color);
    void fillTransparentInPath(const QPainterPath& path, const QColor& color);

    // Selection internals
    void drawSelectionOverlay(QPainter* painter) const;
    void beginRectSelection(const QPointF& start);
    void updateRectSelection(const QPointF& current);
    void beginLassoSelection(const QPointF& start);
    void updateLassoSelection(const QPointF& point);
    void finalizeSelection();
    void extractFloatingFromSelection(bool cut);
    bool pointInSelection(const QPointF& pos) const;

    QImage m_image;                // Main high-resolution image
    QColor m_color;
    QColor m_lineColor;
    QList<QColor> m_lineColors;
    int m_brushSize;
    int m_opacity;
    bool m_fillMode;
    bool m_contiguousSelection;
    bool m_autoLineFill;
    QPointF m_lastPos;
    bool m_drawing;
    
    // Tablet state
    bool m_useTablet;
    bool m_tabletActive;
    qreal m_lastPressure;
    qreal m_lastTiltX;
    qreal m_lastTiltY;
    qreal m_lastRotation;
    
    // Pan and zoom variables
    bool m_isPanning;
    bool m_isSpacePressed;
    bool m_isCtrlPressed;
    QPointF m_panStartPos;
    qreal m_currentScale;
    
    // Track color sampling mode
    bool m_colorSamplingMode;
    
    // Track color replace mode
    bool m_colorReplaceMode;
    
    // Undo/Redo stacks
    QStack<QImage> m_undoStack;
    QStack<QImage> m_redoStack;
    static const int MAX_UNDO_STEPS = 20;
    
    // Brush engine for advanced brush functionality
    BrushEngine *m_brushEngine;
    
    // Custom cursor variables
    bool m_customCursorEnabled;
    QPointF m_cursorPos;
    
    // Lag compensation variables
    static const int POSITION_HISTORY_SIZE = 5;
    QList<QPointF> m_positionHistory;
    QList<qint64> m_timeHistory;

    // New: state for gap fill tools
    bool m_rectGapFillMode = false;
    bool m_lassoGapFillMode = false;
    bool m_lassoUseActiveColor = false; // if false, sample at press
    bool m_isSelecting = false;
    QPointF m_selectStart;
    QPointF m_selectCurrent;
    QColor m_sampledFillColor;
    QPainterPath m_lassoPath;

    // New: selection tool state
    bool m_rectSelectMode = false;
    bool m_lassoSelectMode = false;
    bool m_selectionActive = false;
    QPainterPath m_selectionPath;
    // Floating buffer representing selected pixels during move
    bool m_hasFloating = false;
    QImage m_floatingImage;
    QRect m_floatingBounds; // in canvas coords
    QPointF m_floatingOffset {0, 0};
    bool m_draggingSelection = false;
    QPointF m_dragStartPos;
    QPointF m_dragStartOffset;
    // Marching ants animation
    QTimer* m_antsTimer = nullptr;
    qreal m_antsOffset = 0.0;

    // Navigation input throttling
    // Accumulates pan deltas from high-frequency tablet/mouse events and
    // flushes them at a capped rate (TARGET_NAV_FPS) to avoid wasting CPU
    // on transforms that will never be rendered.
    QTimer* m_navFlushTimer = nullptr;
    QPointF m_pendingPanDelta;          // accumulated pan delta waiting to be applied
    bool m_hasPendingPan = false;
    qreal m_pendingZoomFactor = 1.0;    // accumulated zoom factor (multiplicative)
    bool m_hasPendingZoom = false;
    static constexpr int TARGET_NAV_FPS = 60; // max navigation updates per second

    void flushPendingNavigation();      // apply accumulated delta, called by timer
};

#endif // CANVAS_H 